const puppeteer = require('puppeteer-core');
const TurndownService = require('turndown');
const cheerio = require('cheerio');
const fs = require('fs-extra');
const path = require('path');
const axios = require('axios');

// 配置
const BASE_URL = 'http://etest.kiyun.com/help/';
const OUTPUT_DIR = path.join(__dirname, 'ETest_Help_Docs');
const LOG_FILE = path.join(OUTPUT_DIR, 'download_logs.txt');
const CHROME_PATHS = [
  'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
  `${process.env.LOCALAPPDATA}\\Google\\Chrome\\Application\\chrome.exe`,
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  `${process.env.LOCALAPPDATA}\\Microsoft\\Edge\\Application\\msedge.exe`,
];

// 全局变量
let browser;
let page;
let logs = [];
let turndownService;
let failedImages = [];
let skippedPages = [];

// 初始化Turndown
function initTurndown() {
  turndownService = new TurndownService({
    headingStyle: 'atx',
    codeBlockStyle: 'fenced',
    bulletListMarker: '-',
    strongDelimiter: '**',
    emDelimiter: '*',
  });
  
  // 自定义规则处理图片
  turndownService.addRule('images', {
    filter: 'img',
    replacement: function (content, node) {
      const alt = node.getAttribute('alt') || '';
      const src = node.getAttribute('src') || '';
      return `![${alt}](${src})`;
    }
  });
  
  // 处理代码块
  turndownService.addRule('code', {
    filter: ['pre', 'code'],
    replacement: function (content, node) {
      if (node.nodeName === 'PRE') {
        const code = node.textContent || '';
        return `\`\`\`\n${code}\n\`\`\`\n`;
      }
      return `\`${content}\``;
    }
  });
}

// 查找Chrome路径
async function findChromePath() {
  for (const chromePath of CHROME_PATHS) {
    if (await fs.pathExists(chromePath)) {
      return chromePath;
    }
  }
  throw new Error('未找到Chrome浏览器，请安装Chrome或手动指定路径');
}

// 初始化浏览器
async function initBrowser() {
  const chromePath = await findChromePath();
  console.log(`使用Chrome路径: ${chromePath}`);
  
  browser = await puppeteer.launch({
    executablePath: chromePath,
    headless: 'new',
    args: [
      '--no-sandbox',
      '--disable-setuid-sandbox',
      '--disable-dev-shm-usage',
      '--disable-gpu',
      '--lang=zh-CN,zh',
    ],
    defaultViewport: { width: 1920, height: 1080 },
  });
  
  page = await browser.newPage();
  await page.setUserAgent('Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36');
  await page.setExtraHTTPHeaders({
    'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
  });
}

// 延迟函数
function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// 记录日志
function log(message, type = 'info') {
  const timestamp = new Date().toLocaleString();
  const logMessage = `[${timestamp}] [${type.toUpperCase()}] ${message}`;
  console.log(logMessage);
  logs.push(logMessage);
}

// 下载图片
async function downloadImage(imgUrl, outputDir) {
  try {
    // 处理相对路径
    if (imgUrl.startsWith('/')) {
      imgUrl = new URL(imgUrl, BASE_URL).href;
    }
    
    // 只下载本站图片
    if (!imgUrl.includes('etest.kiyun.com')) {
      return imgUrl;
    }
    
    // 创建图片目录
    const imagesDir = path.join(outputDir, 'images');
    await fs.ensureDir(imagesDir);
    
    // 生成文件名
    const urlObj = new URL(imgUrl);
    const ext = path.extname(urlObj.pathname) || '.png';
    const filename = `${Date.now()}_${Math.random().toString(36).substring(2, 8)}${ext}`;
    const filePath = path.join(imagesDir, filename);
    
    // 下载图片
    const response = await axios({
      url: imgUrl,
      method: 'GET',
      responseType: 'arraybuffer',
      timeout: 10000,
    });
    
    await fs.writeFile(filePath, response.data);
    log(`图片下载成功: ${imgUrl} -> ${filename}`, 'success');
    
    // 返回相对路径
    return `./images/${filename}`;
    
  } catch (error) {
    log(`图片下载失败: ${imgUrl}, 错误: ${error.message}`, 'error');
    failedImages.push({ url: imgUrl, error: error.message });
    return imgUrl; // 下载失败返回原URL
  }
}

// 处理页面内容中的图片
async function processImages(html, outputDir) {
  const $ = cheerio.load(html);
  const imgElements = $('img');
  
  for (let i = 0; i < imgElements.length; i++) {
    const img = imgElements[i];
    const src = $(img).attr('src');
    if (src) {
      const newSrc = await downloadImage(src, outputDir);
      $(img).attr('src', newSrc);
    }
  }
  
  return $.html();
}

// 获取菜单结构
async function getMenuStructure() {
  log('正在获取菜单结构...');
  
  try {
    await page.goto(BASE_URL, { waitUntil: 'networkidle2', timeout: 60000 });
    await delay(8000); // 等待React应用完全加载和菜单渲染
    
    // 提取菜单
    const menu = await page.evaluate(() => {
      const items = [];
      
      // 递归处理菜单
      function processMenu(element, parentPath = [], level = 0) {
        const children = element.children;
        for (let i = 0; i < children.length; i++) {
          const child = children[i];
          
          if (child.tagName === 'LI') {
            const link = child.querySelector('a');
            if (link) {
              const title = link.textContent.trim();
              const url = link.getAttribute('href');
              
              if (url) {
                const fullUrl = new URL(url, window.location.href).href;
                const currentPath = [...parentPath, title];
                
                items.push({
                  title,
                  url: fullUrl,
                  path: currentPath,
                  level,
                });
              }
            }
            
            // 处理子菜单
            const subMenu = child.querySelector('ul');
            if (subMenu) {
              const subTitle = child.querySelector(':scope > a')?.textContent.trim() || '';
              processMenu(subMenu, [...parentPath, subTitle], level + 1);
            }
          }
        }
      }
      
      // 查找左侧菜单（Light UI）
      const menuElement = document.querySelector('aside') || 
                         document.querySelector('.menu') || 
                         document.querySelector('.nav') ||
                         document.querySelector('.sidebar') ||
                         document.querySelector('[class*="menu"]') ||
                         document.querySelector('[class*="sidebar"]') ||
                         document.querySelector('.light-layout-sider') ||
                         document.querySelector('.ant-layout-sider');
      
      if (menuElement) {
        const ul = menuElement.querySelector('ul') || menuElement.querySelector('[class*="menu"]');
        if (ul) {
          processMenu(ul);
        }
      }
      
      // 如果没找到菜单，尝试查找所有可能的导航链接
      if (items.length === 0) {
        const allLinks = document.querySelectorAll('a');
        allLinks.forEach(link => {
          const href = link.getAttribute('href');
          const title = link.textContent.trim();
          if (href && title && href.startsWith('/help/') && !href.includes('#')) {
            const fullUrl = new URL(href, window.location.href).href;
            items.push({
              title,
              url: fullUrl,
              path: [title],
              level: 0,
            });
          }
        });
      }
      
      return items;
    });
    
    // 去重
    const uniqueMenu = [];
    const urls = new Set();
    for (const item of menu) {
      if (!urls.has(item.url) && item.title && !item.title.startsWith('#')) {
        urls.add(item.url);
        uniqueMenu.push(item);
      }
    }
    
    log(`获取到菜单数量: ${uniqueMenu.length}`, 'success');
    return uniqueMenu;
    
  } catch (error) {
    log(`获取菜单失败: ${error.message}`, 'error');
    throw error;
  }
}

// 爬取单个页面
async function scrapePage(item) {
  const { title, url, path } = item;
  log(`正在爬取: ${title} -> ${url}`);
  
  try {
    await page.goto(url, { waitUntil: 'networkidle2', timeout: 30000 });
    await delay(2000);
    
    // 检查是否需要登录
    const needLogin = await page.evaluate(() => {
      const text = document.body.textContent || '';
      return text.includes('登录') && (text.includes('请登录') || text.includes('用户名') || text.includes('密码'));
    });
    
    if (needLogin) {
      log(`页面需要登录，跳过: ${title}`, 'warning');
      skippedPages.push({ title, url, reason: '需要登录' });
      return null;
    }
    
    // 提取正文内容
    const content = await page.evaluate(() => {
      // 尝试找到正文区域
      const contentSelectors = [
        'main',
        '.content',
        '.main-content',
        '.article-content',
        '.doc-content',
        '.markdown-body',
        '#content',
        'article',
      ];
      
      for (const selector of contentSelectors) {
        const element = document.querySelector(selector);
        if (element) {
          // 移除不需要的元素
          const removeSelectors = [
            'nav', 'aside', 'footer', '.header', '.menu', '.sidebar',
            '.toc', '.breadcrumbs', '.pagination', '.comments',
            '.advertisement', '.ads', '.related-articles'
          ];
          
          for (const rmSelector of removeSelectors) {
            const rmElements = element.querySelectorAll(rmSelector);
            rmElements.forEach(el => el.remove());
          }
          
          return element.innerHTML;
        }
      }
      
      // 没找到特定区域，返回body内容
      return document.body.innerHTML;
    });
    
    if (!content || content.trim().length < 100) {
      log(`页面内容过少，跳过: ${title}`, 'warning');
      skippedPages.push({ title, url, reason: '内容过少' });
      return null;
    }
    
    // 创建输出目录
    const outputDir = path.join(OUTPUT_DIR, ...path.slice(0, -1));
    await fs.ensureDir(outputDir);
    
    // 处理图片
    const processedContent = await processImages(content, outputDir);
    
    // HTML转Markdown
    const markdown = turndownService.turndown(processedContent);
    
    // 生成文件名，替换特殊字符
    const safeTitle = title.replace(/[\\/:*?"<>|]/g, '_').trim();
    const fileName = `${safeTitle}.md`;
    const filePath = path.join(outputDir, fileName);
    
    // 保存文件
    await fs.writeFile(filePath, markdown, 'utf8');
    log(`保存成功: ${filePath}`, 'success');
    
    return {
      ...item,
      filePath: path.relative(OUTPUT_DIR, filePath),
    };
    
  } catch (error) {
    log(`爬取页面失败: ${title}, 错误: ${error.message}`, 'error');
    skippedPages.push({ title, url, reason: error.message });
    return null;
  }
}

// 生成索引文件
async function generateIndex(successItems) {
  log('正在生成索引文件...');
  
  let indexContent = `# ETest 帮助中心文档\n\n`;
  indexContent += `## 文档目录\n\n`;
  
  // 按层级组织
  const levels = {};
  for (const item of successItems) {
    const level = item.level;
    if (!levels[level]) {
      levels[level] = [];
    }
    levels[level].push(item);
  }
  
  // 生成索引
  for (const item of successItems) {
    const indent = '  '.repeat(item.level);
    const linkPath = item.filePath.replace(/\\/g, '/');
    indexContent += `${indent}- [${item.title}](${linkPath})\n`;
  }
  
  // 添加统计信息
  indexContent += `\n\n## 统计信息\n\n`;
  indexContent += `- 总页面数量: ${successItems.length + skippedPages.length}\n`;
  indexContent += `- 成功爬取: ${successItems.length}\n`;
  indexContent += `- 跳过页面: ${skippedPages.length}\n`;
  indexContent += `- 失败图片: ${failedImages.length}\n`;
  indexContent += `- 爬取时间: ${new Date().toLocaleString()}\n`;
  
  // 保存索引
  const indexPath = path.join(OUTPUT_DIR, 'README.md');
  await fs.writeFile(indexPath, indexContent, 'utf8');
  log(`索引文件生成成功: ${indexPath}`, 'success');
}

// 生成日志文件
async function generateLogs() {
  log('正在生成日志文件...');
  
  let logContent = logs.join('\n') + '\n\n';
  
  // 跳过的页面
  if (skippedPages.length > 0) {
    logContent += '\n=== 跳过的页面 ===\n';
    for (const page of skippedPages) {
      logContent += `- [${page.title}](${page.url}): ${page.reason}\n`;
    }
  }
  
  // 失败的图片
  if (failedImages.length > 0) {
    logContent += '\n=== 下载失败的图片 ===\n';
    for (const img of failedImages) {
      logContent += `- ${img.url}: ${img.error}\n`;
    }
  }
  
  await fs.writeFile(LOG_FILE, logContent, 'utf8');
  log(`日志文件生成成功: ${LOG_FILE}`, 'success');
}

// 主函数
async function main() {
  try {
    log('=== ETest帮助中心爬取工具开始运行 ===');
    
    // 初始化
    initTurndown();
    await initBrowser();
    await fs.ensureDir(OUTPUT_DIR);
    
    // 获取菜单
    const menu = await getMenuStructure();
    if (menu.length === 0) {
      throw new Error('未获取到任何菜单，请检查页面结构是否变化');
    }
    
    // 爬取所有页面
    const successItems = [];
    for (let i = 0; i < menu.length; i++) {
      const item = menu[i];
      log(`进度: ${i + 1}/${menu.length}`, 'info');
      const result = await scrapePage(item);
      if (result) {
        successItems.push(result);
      }
      await delay(1500); // 爬取间隔，避免被封
    }
    
    // 生成索引和日志
    await generateIndex(successItems);
    await generateLogs();
    
    // 统计结果
    log('=== 爬取完成 ===', 'success');
    log(`总页面数: ${menu.length}`, 'info');
    log(`成功爬取: ${successItems.length}`, 'success');
    log(`跳过页面: ${skippedPages.length}`, 'warning');
    log(`失败图片: ${failedImages.length}`, 'warning');
    log(`文档保存在: ${OUTPUT_DIR}`, 'success');
    
  } catch (error) {
    log(`程序运行失败: ${error.message}`, 'error');
    console.error(error);
  } finally {
    if (browser) {
      await browser.close();
    }
    process.exit(0);
  }
}

// 运行主函数
main();
