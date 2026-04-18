const puppeteer = require('puppeteer-core');
const fs = require('fs-extra');
const path = require('path');

const BASE_URL = 'http://etest.kiyun.com/help/';
const CHROME_PATHS = [
  'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
  `${process.env.LOCALAPPDATA}\\Google\\Chrome\\Application\\chrome.exe`,
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  `${process.env.LOCALAPPDATA}\\Microsoft\\Edge\\Application\\msedge.exe`,
];

// 查找Chrome路径
async function findChromePath() {
  for (const chromePath of CHROME_PATHS) {
    if (await fs.pathExists(chromePath)) {
      return chromePath;
    }
  }
  throw new Error('未找到Chrome浏览器，请安装Chrome或手动指定路径');
}

async function main() {
  const chromePath = await findChromePath();
  console.log(`使用浏览器: ${chromePath}`);
  
  const browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: 'new',
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
    defaultViewport: { width: 1920, height: 1080 },
  });
  
  const page = await browser.newPage();
  await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: 30000 });
  await new Promise(resolve => setTimeout(resolve, 5000));
  
  // 获取完整HTML
  const html = await page.content();
  await fs.writeFile('page.html', html, 'utf8');
  console.log('页面HTML已保存到page.html');
  
  // 尝试查找所有菜单相关元素
  const selectors = [
    'aside', '.sidebar', '.menu', '.nav', '.navigation',
    '#menu', '#sidebar', '#nav', '.ant-menu', '.el-menu',
    '.ivu-menu', '.doc-menu', '.help-menu'
  ];
  
  console.log('\n查找菜单元素:');
  for (const selector of selectors) {
    const count = await page.evaluate((sel) => {
      return document.querySelectorAll(sel).length;
    }, selector);
    console.log(`${selector}: ${count} 个元素`);
  }
  
  // 输出页面中的所有链接
  const links = await page.evaluate(() => {
    const allLinks = [];
    document.querySelectorAll('a').forEach(a => {
      const href = a.getAttribute('href');
      const text = a.textContent.trim();
      if (href && text) {
        allLinks.push({ text, href });
      }
    });
    return allLinks;
  });
  
  console.log('\n页面中的链接:');
  links.forEach(link => {
    console.log(`- ${link.text}: ${link.href}`);
  });
  
  await browser.close();
}

main();
