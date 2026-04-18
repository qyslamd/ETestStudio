
const fs = require('fs-extra');
const path = require('path');
const axios = require('axios');
const TurndownService = require('turndown');

// 配置
const DB_PATH = 'C:\\Users\\zhouy\\.local\\share\\opencode\\tool-output\\tool_d9c5853cc0013Pj9fj16SIr137';
const OUTPUT_DIR = path.join(__dirname, 'ETest_Help_Docs');
const BASE_URL = 'http://etest.kiyun.com/help';

// 全局变量
const turndownService = new TurndownService({
  headingStyle: 'atx',
  codeBlockStyle: 'fenced',
  bulletListMarker: '-',
  strongDelimiter: '**',
  emDelimiter: '*',
});
const logs = [];
const failedImages = [];

// 初始化Turndown图片处理
turndownService.addRule('images', {
  filter: 'img',
  replacement: function (content, node) {
    const alt = node.getAttribute('alt') || '';
    const src = node.getAttribute('src') || '';
    return `![${alt}](${src})`;
  }
});

// 日志函数
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
    } else if (imgUrl.startsWith('./')) {
      imgUrl = `${BASE_URL}${imgUrl.slice(1)}`;
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

// 处理文档内容中的图片
async function processImages(content, outputDir) {
  // 匹配所有图片标签
  const imgRegex = /<img[^>]+src="([^"]+)"[^>]*>/g;
  let match;
  const imgMap = new Map();

  // 收集所有图片URL
  while ((match = imgRegex.exec(content)) !== null) {
    const imgUrl = match[1];
    if (!imgMap.has(imgUrl)) {
      const localUrl = await downloadImage(imgUrl, outputDir);
      imgMap.set(imgUrl, localUrl);
    }
  }

  // 替换图片URL为本地路径
  let processedContent = content;
  for (const [originalUrl, localUrl] of imgMap.entries()) {
    processedContent = processedContent.replace(new RegExp(originalUrl.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g'), localUrl);
  }

  return processedContent;
}

// HTML转Markdown
function htmlToMarkdown(html) {
  return turndownService.turndown(html);
}

// 递归处理目录结构
async function processCatalog(catalog, parentPath = []) {
  const title = String(catalog.title || '未命名').replace(/[\\/:*?"<>|]/g, '_').trim();
  
  // 如果当前目录有文档或者API，直接作为文件保存，不需要创建子目录
  if (catalog.documentId || (catalog.type === 'api' && catalog.api)) {
    const outputDir = path.join(OUTPUT_DIR, ...parentPath);
    await fs.ensureDir(outputDir);
    
    log(`处理文档: ${[...parentPath, title].join('/')}`, 'info');

    // 处理当前目录的文档
    if (catalog.documentId) {
      try {
        // 从documents表获取文档文件名
        const doc = documents.find(d => d.id === catalog.documentId);
        if (doc && doc.value) {
          // 请求获取Markdown内容
          const docUrl = `http://etest.kiyun.com/help/documents/${doc.value}`;
          const response = await axios.get(docUrl, { timeout: 10000 });
          let markdown = response.data;
          
          // 处理图片
          const imgRegex = /!\[.*?\]\((\.\/db-images\/.*?)\)/g;
          let match;
          const imgMap = new Map();
          
          // 收集所有图片URL
          while ((match = imgRegex.exec(markdown)) !== null) {
            const imgRelativePath = match[1];
            if (!imgMap.has(imgRelativePath)) {
              const imgUrl = `http://etest.kiyun.com/help${imgRelativePath.substring(1)}`;
              try {
                // 创建图片目录
                const imagesDir = path.join(outputDir, 'images');
                await fs.ensureDir(imagesDir);
                
                // 生成文件名
                const imgName = path.basename(imgRelativePath);
                const imgPath = path.join(imagesDir, imgName);
                
                // 下载图片
                const imgResponse = await axios.get(imgUrl, { responseType: 'arraybuffer', timeout: 10000 });
                await fs.writeFile(imgPath, imgResponse.data);
                
                // 替换图片路径
                imgMap.set(imgRelativePath, `./images/${imgName}`);
                log(`图片下载成功: ${imgUrl}`, 'success');
              } catch (imgError) {
                log(`图片下载失败: ${imgUrl}, 错误: ${imgError.message}`, 'error');
                failedImages.push({ url: imgUrl, error: imgError.message });
                // 下载失败保留原路径
                imgMap.set(imgRelativePath, imgUrl);
              }
            }
          }
          
          // 替换所有图片路径
          for (const [originalPath, newPath] of imgMap.entries()) {
            markdown = markdown.replace(new RegExp(originalPath.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'g'), newPath);
          }
          
          // 保存文件
          const fileName = `${title}.md`;
          const filePath = path.join(outputDir, fileName);
          await fs.writeFile(filePath, markdown, 'utf8');
          log(`保存文档成功: ${filePath}`, 'success');
          catalog.localPath = path.relative(OUTPUT_DIR, filePath).replace(/\\/g, '/');
        }
      } catch (error) {
        log(`处理文档失败: ${catalog.title}, 错误: ${error.message}`, 'error');
      }
    }

    // 处理API
    if (catalog.type === 'api' && catalog.api) {
      try {
        const api = catalog.api;
        let mdContent = `# ${api.name}\n\n`;
        mdContent += `## 描述\n${api.memo || ''}\n\n`;
        
        if (api.params && api.params.length > 0) {
          mdContent += `## 输入参数\n\n`;
          mdContent += `| 参数名 | 类型 | 描述 |\n`;
          mdContent += `| --- | --- | --- |\n`;
          for (const param of api.params) {
            mdContent += `| ${param.name} | ${param.kind} | ${param.memo || ''} |\n`;
            if (param.details && param.details.length > 0) {
              for (const detail of param.details) {
                mdContent += `| &nbsp;&nbsp;${detail.name} | ${detail.kind} | ${detail.memo || ''} |\n`;
              }
            }
          }
          mdContent += `\n`;
        }

        if (api.return && api.return.length > 0) {
          mdContent += `## 返回值\n\n`;
          mdContent += `| 参数名 | 类型 | 描述 |\n`;
          mdContent += `| --- | --- | --- |\n`;
          for (const ret of api.return) {
            mdContent += `| ${ret.name} | ${ret.kind} | ${ret.memo || ''} |\n`;
            if (ret.details && ret.details.length > 0) {
              for (const detail of ret.details) {
                mdContent += `| &nbsp;&nbsp;${detail.name} | ${detail.kind} | ${detail.memo || ''} |\n`;
              }
            }
          }
          mdContent += `\n`;
        }

        if (api.example) {
          mdContent += `## 示例\n\n\`\`\`lua\n${api.example}\n\`\`\`\n`;
        }

        const fileName = `${title}.md`;
        const filePath = path.join(outputDir, fileName);
        await fs.writeFile(filePath, mdContent, 'utf8');
        log(`保存API文档成功: ${filePath}`, 'success');
        catalog.localPath = path.relative(OUTPUT_DIR, filePath).replace(/\\/g, '/');
      } catch (error) {
        log(`处理API文档失败: ${catalog.title}, 错误: ${error.message}`, 'error');
      }
    }
  }

  // 如果有子目录，需要创建当前目录并处理子目录
  if (catalog.children && catalog.children.length > 0) {
    const currentPath = [...parentPath, title];
    const outputDir = path.join(OUTPUT_DIR, ...currentPath);
    await fs.ensureDir(outputDir);
    
    log(`处理目录: ${currentPath.join('/')}`, 'info');
    
    for (const child of catalog.children) {
      await processCatalog(child, currentPath);
    }
  }

  return catalog;
}

// 生成索引文件
async function generateIndex(catalogs) {
  log(`生成索引文件`, 'info');
  let indexContent = `# ETest 帮助中心文档\n\n`;
  indexContent += `## 目录\n\n`;

  // 递归生成目录结构
  function generateToc(catalog, level = 0) {
    const indent = '  '.repeat(level);
    let toc = '';
    if (catalog.localPath) {
      toc += `${indent}- [${catalog.title}](${catalog.localPath})\n`;
    } else {
      toc += `${indent}- ${catalog.title}\n`;
    }
    if (catalog.children && catalog.children.length > 0) {
      for (const child of catalog.children) {
        toc += generateToc(child, level + 1);
      }
    }
    return toc;
  }

  for (const catalog of catalogs) {
    indexContent += generateToc(catalog);
  }

  // 添加统计信息
  indexContent += `\n\n## 统计信息\n\n`;
  indexContent += `- 总目录数: ${catalogs.length}\n`;
  indexContent += `- 总文档数: ${documents.length}\n`;
  indexContent += `- 总API数: ${apis.length}\n`;
  indexContent += `- 下载失败的图片数: ${failedImages.length}\n`;
  indexContent += `- 生成时间: ${new Date().toLocaleString()}\n`;

  const indexPath = path.join(OUTPUT_DIR, 'README.md');
  await fs.writeFile(indexPath, indexContent, 'utf8');
  log(`索引文件生成成功: ${indexPath}`, 'success');
}

// 生成日志文件
async function generateLogs() {
  log(`生成日志文件`, 'info');
  let logContent = logs.join('\n') + '\n\n';

  if (failedImages.length > 0) {
    logContent += `\n=== 下载失败的图片 ===\n`;
    for (const img of failedImages) {
      logContent += `- ${img.url}: ${img.error}\n`;
    }
  }

  const logPath = path.join(OUTPUT_DIR, 'download_logs.txt');
  await fs.writeFile(logPath, logContent, 'utf8');
  log(`日志文件生成成功: ${logPath}`, 'success');
}

// 主函数
async function main() {
  try {
    log(`=== ETest帮助中心文档导出开始 ===`, 'info');

    // 读取数据库文件
    const dbContent = await fs.readFile(DB_PATH, 'utf8');
    const db = JSON.parse(dbContent);

    // 提取数据
    const collections = db.collections || [];
    global.catalogs = collections.find(c => c.name === 'catalog')?.data || []; // 目录结构
    global.documents = collections.find(c => c.name === 'document')?.data || []; // 文档内容
    global.apis = collections.find(c => c.name === 'api')?.data || []; // API内容

    log(`读取数据库成功，共 ${catalogs.length} 个目录，${documents.length} 个文档，${apis.length} 个API`, 'success');

    // 关联API到目录
    for (const catalog of catalogs) {
      if (catalog.type === 'api' && catalog.apiId) {
        catalog.api = apis.find(a => a.id === catalog.apiId);
      }
      if (catalog.children) {
        for (const child of catalog.children) {
          if (child.type === 'api' && child.apiId) {
            child.api = apis.find(a => a.id === child.apiId);
          }
        }
      }
    }

    // 创建输出目录
    await fs.ensureDir(OUTPUT_DIR);

    // 处理所有目录
    const processedCatalogs = [];
    for (const catalog of catalogs) {
      const processed = await processCatalog(catalog);
      processedCatalogs.push(processed);
    }

    // 生成索引和日志
    await generateIndex(processedCatalogs);
    await generateLogs();

    log(`=== 文档导出完成 ===`, 'success');
    log(`所有文档已保存到: ${OUTPUT_DIR}`, 'success');

  } catch (error) {
    log(`程序运行失败: ${error.message}`, 'error');
    console.error(error);
    process.exit(1);
  }
}

// 运行主函数
main();
