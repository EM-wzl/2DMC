/* 本地静态服务器：node test/serve.cjs [端口] —— 用 http 打开才能使用局域网联机 */
const http = require('http');
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
const port = Number(process.argv[2] || 8765);
const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
               '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8',
               '.cjs': 'text/plain; charset=utf-8', '.md': 'text/plain; charset=utf-8' };

http.createServer((req, res) => {
  let rel = decodeURIComponent(req.url.split('?')[0]);
  if (rel === '/' || rel === '') rel = '/index.html';
  const file = path.join(root, path.normalize(rel).replace(/^([/\\]|\.\.[/\\])+/, ''));
  if (!file.startsWith(root)) { res.writeHead(403); res.end('forbidden'); return; }
  fs.readFile(file, (err, buf) => {
    if (err) { res.writeHead(404); res.end('not found'); return; }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream',
                         'Cache-Control': 'no-store' });
    res.end(buf);
  });
}).listen(port, '127.0.0.1', () => {
  console.log('2DMC Web: http://127.0.0.1:' + port + '/');
});
