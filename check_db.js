const fs = require('fs-extra');
const dbContent = fs.readFileSync('C:\\Users\\zhouy\\.local\\share\\opencode\\tool-output\\tool_d9c5853cc0013Pj9fj16SIr137', 'utf8');
const db = JSON.parse(dbContent);
const documents = db.collections.find(c => c.name === 'document').data;
const apis = db.collections.find(c => c.name === 'api').data;

// 查看第一个document的结构
console.log('第一个document的结构：', JSON.stringify(documents[0], null, 2));
console.log('第二个document的结构：', JSON.stringify(documents[1], null, 2));

// 查看第一个api的结构
console.log('第一个api的结构：', JSON.stringify(apis[0], null, 2));

// 查看所有集合名称
console.log('所有集合：', db.collections.map(c => c.name));
