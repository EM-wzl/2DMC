/* Node 自检：用 vm 加载纯逻辑模块，跑与浏览器 /selftest 完全相同的断言 */
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const webRoot = path.join(__dirname, '..');
const files = ['core', 'items', 'inventory', 'world', 'entities', 'selftest'];

const ctx = { console, Buffer, setTimeout, clearTimeout };
vm.createContext(ctx);

for (const name of files) {
  const file = path.join(webRoot, 'js', name + '.js');
  vm.runInContext(fs.readFileSync(file, 'utf8'), ctx, { filename: file });
}

const r = vm.runInContext('DMC.SelfTest.run()', ctx);
for (const line of r.lines) {
  if (line.startsWith('FAIL')) console.log('  ' + line);
}
console.log(`\n2DMC Web 自检：${r.passed} 通过 / ${r.failed} 失败`);
if (process.argv.includes('--verbose')) {
  console.log('---- 明细 ----');
  r.lines.forEach(l => console.log('  ' + l));
}
process.exit(r.failed === 0 ? 0 : 1);
