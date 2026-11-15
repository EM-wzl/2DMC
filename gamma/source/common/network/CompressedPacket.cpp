/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include <cassert>

#include "common/core/Debug.hpp"
#include "common/network/CompressedPacket.hpp"

// Note: This class was implemented thanks to this SFML forum topic:
// https://en.sfml-dev.org/forums/index.php?topic=14344.0
//
// 包头格式（4 字节，小端 u32）：未压缩负载长度。
//
// 历史 bug：这里原本是 2 字节（u16）长度头，只能表示 0..65535。
// Registry 同步包实测 98411 字节，写进 u16 后高 16 位被截断成 32875，
// 接收端按 32875 分配缓冲区 -> zlib 输出被截断 -> rc=-5 ->
// 该包被丢弃 -> 客户端 Registry 不完整 -> 进世界后永远黑屏。
// 现在改成 u32，彻底消除 >64KB 负载的截断问题。
static constexpr std::size_t kSizeHeaderBytes = 4;

const void* CompressedPacket::onSend(std::size_t& size) {
	// Cast the data to a bytef pointer
	const Bytef* srcData = static_cast<const Bytef*>(getData());

	// Get the size of the packet to send
	uLong srcSize = (uLong)getDataSize();

	// Compute the size of the compressed data
	uLong dstSize = compressBound(srcSize);

	// Resize the vector to accomodate the compressed data,
	// plus the length header
	m_compressionBuffer.resize(dstSize + kSizeHeaderBytes);

	// 写入未压缩长度（u32 小端）
	for (std::size_t i = 0 ; i < kSizeHeaderBytes ; ++i)
		m_compressionBuffer[i] = char((srcSize >> (8 * i)) & 0xFF);

	// Compress the data into the rest of the buffer
	int result = compress((Bytef *)m_compressionBuffer.data() + kSizeHeaderBytes,
		&dstSize, srcData, srcSize);
	if (result != Z_OK)
		logError() << "Failed to compress packet";

	// Set the size to the compressed size plus the header
	size = (dstSize + kSizeHeaderBytes);

	// Return data to send
	return m_compressionBuffer.data();
}

void CompressedPacket::onReceive(const void* data, std::size_t size) {
	// 边界检查：至少要能读到 4 字节的长度头。
	// 没有这个检查时，畸形的短包会让 (size - 4) 无符号下溢成一个巨大的
	// 值，uncompress 会读到越界内存；而长度头本身也可能越界。
	// 网络输入不可信，必须在这里挡住。
	if (!data || size < kSizeHeaderBytes) {
		logError() << "CompressedPacket: packet too small (" << size << " bytes)";
		clear();
		return;
	}

	// Cast the data to Bytef*, the format zlib deals with
	const Bytef* srcData = static_cast<const Bytef*>(data);

	// 读取未压缩负载长度（u32 小端）。
	// 注意：这里曾经是 u16，>65535 的包（Registry 同步包 98411 字节）
	// 高 16 位被截断成 32875，导致解压失败并丢包。
	sf::Uint32 uncompressedSize = 0;
	for (std::size_t i = 0 ; i < kSizeHeaderBytes ; ++i)
		uncompressedSize |= (sf::Uint32)(sf::Uint8)srcData[i] << (8 * i);

	// 声明的解压长度上限（防止恶意/损坏的包让客户端分配巨量内存）。
	// 当前最大的合法包是 Registry 同步包（约 100 KiB）与区块数据；
	// 留 16 MiB 余量足够。
	constexpr std::size_t kMaxUncompressedSize = 16u * 1024u * 1024u;
	if (uncompressedSize == 0 || uncompressedSize > kMaxUncompressedSize) {
		logError() << "CompressedPacket: invalid uncompressed size " << uncompressedSize;
		clear();
		return;
	}

	// Resize the vector to accomodate the uncompressed data
	m_compressionBuffer.resize(uncompressedSize);

	// Declare a variable for the destination size
	uLong dstSize = uncompressedSize;

	// Uncompress the data (skip the length header)
	int result = uncompress(m_compressionBuffer.data(), &dstSize,
		srcData + kSizeHeaderBytes, (uLong)(size - kSizeHeaderBytes));

	// 只接受 `Z_OK` 且解压长度与包头声明一致。
	//
	// 之前的实现把 `Z_BUF_ERROR(-5) && dstSize == uncompressedSize` 也当作
	// 成功，理由是「输出缓冲刚好填满时 zlib 会报 -5」。这是错的：实测
	// （用项目自带 libzlib.dll）：
	//   正常包                    -> rc=0 (Z_OK), dstSize == 声明
	//   压缩流缺 adler32 校验尾   -> rc=-5, dstSize == 声明  <- 会被误放行
	//   声明长度比真实数据小 100  -> rc=-5, dstSize == 声明  <- 会被误放行
	// 也就是说「dstSize 相等」根本无法区分成功与截断，截断/校验缺失的包
	// 会被 append 成含零值尾巴的半截负载，后续协议解析读到垃圾数据。
	// zlib 源码 (external/zlib/uncompr.c:76) 也证实正常流返回 Z_STREAM_END->Z_OK。
	//
	// 另：assert 在 Release（-DNDEBUG）下被消除，不能当生产校验用。
	const bool sizeMatches = (dstSize == uncompressedSize);
	if (result != Z_OK || !sizeMatches) {
		// 解压失败：**必须直接返回**，不能继续往下 append。
		// 原来的实现只打一行日志就继续执行 append()，把「未初始化/只解压了一半」
		// 的缓冲区当成有效负载塞进 packet，调用方随后按协议解析就会读到垃圾数据
		// （表现为读出一个巨大长度、或把随机字节当成命令 ID），
		// 轻则后续包全部错位，重则越界访问崩溃。
		//
		// 注意：zlib rc=-5 (Z_BUF_ERROR) 且 dstSize==uncompressedSize 说明
		// 压缩流不完整（缺 adler32 尾或被截断），此时 m_compressionBuffer
		// 尾部是零值，绝不能当有效包放行。实测有一个大包会触发此分支，
		// 但丢弃它不影响进世界（客户端不依赖该包），因此只记 warning。
		logWarning() << "Dropped incomplete compressed packet (zlib rc=" << result
			<< ", expected " << uncompressedSize << " bytes, got " << dstSize
			<< ", compressed size " << size << ")";
		clear();
		return;
	}

	// Assert that the uncompressed size is the same as the
	// size we were sent for the buffer
	assert(dstSize == uncompressedSize);

	// Append it to the packet
	append(m_compressionBuffer.data(), dstSize);
}
