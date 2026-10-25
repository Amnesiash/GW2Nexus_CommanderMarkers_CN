#!/usr/bin/env node
'use strict';
/**
 * 语法与一致性静态自检（无法在本机编译时的替代验证）。
 * 用法: node scripts/verify_i18n.js
 *
 * 检查项：
 *  1. locale/*.inc 是合法的 initializer_list 片段（含跨行条目、无 BOM、无重复键）。
 *  2. 每个源码 T()/Ts() 的键在译表中存在（缺失只警告，运行时回退英文）。
 *  3. 译表中没有被使用的键（警告，提示可清理）。
 *  4. T() 没有误用在 ImGui 内部 ID（##xxx）或纯格式串上。
 *  5. 使用了 T() 的文件都有 #include "core/Locale.h" 与 using cm::Locale::T;。
 *  6. static 数组里没有 T()（切换语言不会刷新）。
 *  7. printf 格式符一致性（译文丢失/新增 % 会导致 ImGui 读越界参数而崩溃）。
 *
 * 间接包装识别（避免把正常写法误报为「未被使用」）：
 *  - for (const char* x : {"a","b"}) ... T(x)
 *  - helper(const char* title, ...) 调用点 helper("a", ...) 且函数体内 T(title)
 *  - T(kConstant)，常量值从 core/Branding.h 解析
 *  - Ts(...) 视同 T(...)
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const LOCALE_DIR = path.join(ROOT, 'src', 'core', 'locale');
const SRC_DIR = path.join(ROOT, 'src');

// Branding constants (kDescription, kDisplayName, ...) are referenced as
// T(kDescription) instead of a literal, so their values are read from here.
const brandingHPath = path.join(ROOT, 'src', 'core', 'Branding.h');
const brandingH = fs.existsSync(brandingHPath) ? fs.readFileSync(brandingHPath, 'utf8') : '';

const problems = [];
const warnings = [];
function fail(file, msg) { problems.push(`${file}: ${msg}`); }
function warn(file, msg) { warnings.push(`${file}: ${msg}`); }

function listCppFiles() {
  const out = [];
  const walk = (dir) => {
    for (const name of fs.readdirSync(dir)) {
      const full = path.join(dir, name);
      if (fs.statSync(full).isDirectory()) { walk(full); continue; }
      if (/\.(cpp|h)$/.test(name)) out.push(full);
    }
  };
  walk(SRC_DIR);
  return out;
}

function stripComments(src) {
  return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

function unescapeLiteral(raw) {
  return raw.replace(/\\(.)/g, (mm, ch) => {
    const map = { n: '\n', t: '\t', r: '\r', '0': '\0', '"': '"', '\\': '\\', "'": "'", '?': '?' };
    return Object.prototype.hasOwnProperty.call(map, ch) ? map[ch] : ch;
  });
}

/* ---------- 1. 解析 .inc ---------- */
const table = new Map();
const duplicateKeys = new Set();
let incEntryCount = 0;

for (const name of fs.readdirSync(LOCALE_DIR).filter(n => n.endsWith('.inc')).sort()) {
  const full = path.join(LOCALE_DIR, name);
  const src = fs.readFileSync(full, 'utf8');
  const lines = src.split(/\r?\n/);

  // 合并跨行条目：从 "{" 起，按花括号配平闭合。
  const logical = [];
  let depth = 0, inEntry = false, buf = [], startLine = 0;
  for (let i = 0; i < lines.length; i++) {
    const trimmed = lines[i].trim();
    if (!inEntry) {
      if (trimmed === '' || trimmed.startsWith('//')) continue;
      if (trimmed.startsWith('{')) { inEntry = true; depth = 0; buf = []; startLine = i + 1; }
      else { fail(name, `第 ${i + 1} 行不是合法条目: ${trimmed.slice(0, 90)}`); continue; }
    }
    buf.push(lines[i]);
    for (const ch of lines[i]) {
      if (ch === '{') depth++;
      else if (ch === '}') depth--;
    }
    if (depth <= 0) { logical.push({ text: buf.join('\n').trim(), line: startLine }); inEntry = false; buf = []; }
  }
  if (inEntry) fail(name, `第 ${startLine} 行条目未闭合`);

  for (const entry of logical) {
    const m = /^\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\},?$/s.exec(entry.text);
    if (!m) {
      fail(name, `第 ${entry.line} 行条目格式错误: ${entry.text.slice(0, 100).replace(/\n/g, ' ')}`);
      continue;
    }
    const en = unescapeLiteral(m[1]);
    const zh = unescapeLiteral(m[2]);
    if (en.trim() === '') fail(name, `第 ${entry.line} 行英文原文为空`);
    if (zh.trim() === '') fail(name, `第 ${entry.line} 行译文为空: "${en.slice(0, 60)}"`);
    if (en === zh) warn(name, `第 ${entry.line} 行译文与原文相同: "${en.slice(0, 50)}"`);
    if (table.has(en)) {
      duplicateKeys.add(en);
      warn(name, `重复键 "${en.slice(0, 50)}"（另一处在 ${table.get(en).file}）`);
    }
    table.set(en, { zh, file: name });
    incEntryCount++;
  }

  const bytes = fs.readFileSync(full);
  if (bytes[0] === 0xEF && bytes[1] === 0xBB && bytes[2] === 0xBF) {
    fail(name, '文件带 UTF-8 BOM，请去掉');
  }
}

/* ---------- 2. 扫描源码中的 T()/Ts() ---------- */
const usedKeys = new Map();
let tCallCount = 0;

for (const full of listCppFiles()) {
  const rel = path.relative(ROOT, full).replace(/\\/g, '/');
  let src;
  try { src = fs.readFileSync(full, 'utf8'); } catch { continue; }
  const plain = stripComments(src);
  const brandingSrc = stripComments(brandingH);

  // Locale 模块自身：形如 T(const char* text) 是声明/定义，不是调用。
  if (/src[\\/]core[\\/]Locale\.(cpp|h)$/.test(full)) continue;
  if (!/\bT\(/.test(plain)) continue;

  if (!/#include\s+"core\/Locale\.h"/.test(plain)) {
    fail(rel, '使用了 T() 但未 #include "core/Locale.h"');
  }
  if (!/using\s+cm::Locale::T\s*;/.test(plain)) {
    fail(rel, '使用了 T() 但缺少 using cm::Locale::T;');
  }

  // 直接包装：T("...") / Ts("...")，先合并相邻字面量。
  const re = /\bTs?\s*\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)/g;
  let m;
  while ((m = re.exec(plain)) !== null) {
    tCallCount++;
    const parts = [];
    const lit = /"((?:[^"\\]|\\.)*)"/g;
    let l;
    while ((l = lit.exec(m[1])) !== null) parts.push(unescapeLiteral(l[1]));
    const key = parts.join('');
    if (key.startsWith('##') || key.startsWith('#')) {
      fail(rel, `T() 包装了 ImGui 内部 ID: "${key}"`);
      continue;
    }
    if (/^%[-0-9.]*[sdfx]/.test(key) && key.replace(/%[-0-9.]*[sdfx]/g, '').trim() === '') {
      fail(rel, `T() 包装了纯格式串: "${key}"`);
      continue;
    }
    if (key === '') continue;
    if (!usedKeys.has(key)) usedKeys.set(key, []);
    usedKeys.get(key).push(rel);
    if (!table.has(key)) warn(rel, `T("${key.slice(0, 70)}") 不在译表中，将回退英文`);
  }

  // 间接包装：T(variable)
  const T_ARG_RE = /\bTs?\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)/g;
  const varNames = new Set();
  while ((m = T_ARG_RE.exec(plain)) !== null) varNames.add(m[1]);

  // 辅助函数形参：helper(const char* title, ...) 且体内 T(title)
  const funcParamRe = /([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*const\s+char\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*,/g;
  const helperParams = new Map();
  while ((m = funcParamRe.exec(plain)) !== null) helperParams.set(m[2], m[1]);

  for (const varName of varNames) {
    // 形态 1：for-range 初始化列表
    const declRe = new RegExp(
      'for\\s*\\(\\s*const\\s+char\\s*\\*\\s*' + varName + '\\s*:\\s*\\{([^}]*)\\}', 'g');
    let d;
    while ((d = declRe.exec(plain)) !== null) {
      const lit = /"((?:[^"\\]|\\.)*)"/g;
      let l;
      while ((l = lit.exec(d[1])) !== null) {
        const key = unescapeLiteral(l[1]);
        if (key.startsWith('#') || key === '') continue;
        if (!usedKeys.has(key)) usedKeys.set(key, []);
        if (!usedKeys.get(key).includes(rel)) usedKeys.get(key).push(rel);
        if (!table.has(key)) warn(rel, `T() 间接引用的 "${key.slice(0, 70)}" 不在译表中，将回退英文`);
      }
    }

    // 形态 2：辅助函数形参，真实字面量在调用点
    if (helperParams.has(varName)) {
      const funcName = helperParams.get(varName);
      const callRe = new RegExp('\\b' + funcName + '\\s*\\(\\s*"((?:[^"\\\\]|\\\\.)*)"', 'g');
      let c;
      while ((c = callRe.exec(plain)) !== null) {
        const key = unescapeLiteral(c[1]);
        if (key.startsWith('#') || key === '') continue;
        if (!usedKeys.has(key)) usedKeys.set(key, []);
        if (!usedKeys.get(key).includes(rel)) usedKeys.get(key).push(rel);
        if (!table.has(key)) warn(rel, `T() 间接引用的 "${key.slice(0, 70)}" 不在译表中，将回退英文`);
      }
    }

    // 形态 3：T(kConstant)，常量值来自 core/Branding.h
    const brandMatch = new RegExp(
      'constexpr\\s+const\\s+char\\s*\\*\\s*' + varName + '\\s*=\\s*((?:"(?:[^"\\\\]|\\\\.)*"\\s*)+);'
    ).exec(brandingSrc);
    if (brandMatch) {
      const lit = /"((?:[^"\\]|\\.)*)"/g;
      let l;
      const parts = [];
      while ((l = lit.exec(brandMatch[1])) !== null) parts.push(unescapeLiteral(l[1]));
      const key = parts.join('');
      if (!key.startsWith('#') && key !== '') {
        if (!usedKeys.has(key)) usedKeys.set(key, []);
        if (!usedKeys.get(key).includes(rel)) usedKeys.get(key).push(rel);
        if (!table.has(key)) warn(rel, `T() 引用的常量 ${varName} = "${key.slice(0, 70)}" 不在译表中，将回退英文`);
      }
    }
  }

  // static 数组里含 T()
  const staticArrRe = /static\s+const\s+char\s*\*\s*\w+\s*\[\s*\][^=]*=\s*\{[^}]*\bT\s*\(/g;
  if (staticArrRe.test(plain)) {
    fail(rel, '存在 static const char* 数组内含 T()，切换语言不会刷新，请去掉 static');
  }
}

let unusedKeys = 0;
for (const key of table.keys()) {
  if (!usedKeys.has(key)) {
    unusedKeys++;
    warn('locale', `译表键未被任何 T() 使用: "${key.slice(0, 70)}"`);
  }
}

/* ---------- 3. LocaleStrings.h include 齐备 ---------- */
const stringsH = fs.readFileSync(path.join(ROOT, 'src', 'core', 'LocaleStrings.h'), 'utf8');
for (const name of fs.readdirSync(LOCALE_DIR).filter(n => n.endsWith('.inc'))) {
  if (!stringsH.includes(`locale/${name}`)) {
    fail('LocaleStrings.h', `未 include core/locale/${name}`);
  }
}

/* ---------- 4. printf 格式符一致性 ---------- */
const SPEC = /%[-+ #0]*\*?(?:\d+)?(?:\.\d+)?(?:hh|h|ll|l|L|z|j|t)?(?:s|d|i|u|o|x|X|f|F|e|E|g|G|a|A|c|p|n|%)/g;
const specsOf = (s) => (s.match(SPEC) || []).map((x) => x.replace(/%%/, '%'));
let formatSpecErrors = 0;

for (const [en, meta] of table) {
  const enSpecs = specsOf(en).join('|');
  const zhSpecs = specsOf(meta.zh).join('|');
  if (enSpecs !== zhSpecs) {
    formatSpecErrors++;
    fail(meta.file, `格式符不一致 EN=[${enSpecs}] ZH=[${zhSpecs}] : "${en.slice(0, 60)}"`);
  }
  if (meta.zh.replace(SPEC, '').includes('%')) {
    formatSpecErrors++;
    fail(meta.file, `译文含游离 '%'（会被 ImGui 当格式符）: "${meta.zh.slice(0, 60)}"`);
  }
}

/* ---------- 报告 ---------- */
console.log('=== i18n 自检报告 ===');
console.log(`.inc 译文条目 : ${incEntryCount}`);
console.log(`重复英文键   : ${duplicateKeys.size}`);
console.log(`源码 T() 调用: ${tCallCount}`);
console.log(`被使用键数   : ${usedKeys.size}`);
console.log(`未使用译表键 : ${unusedKeys}`);
console.log(`格式符错误   : ${formatSpecErrors}`);
console.log(`错误         : ${problems.length}`);
console.log(`警告         : ${warnings.length}`);
console.log('');
if (problems.length) {
  for (const p of problems.slice(0, 60)) console.log('  [ERR] ' + p);
  if (problems.length > 60) console.log(`  ... 另有 ${problems.length - 60} 条`);
}
if (warnings.length) {
  for (const w of warnings.slice(0, 60)) console.log('  [WARN] ' + w);
  if (warnings.length > 60) console.log(`  ... 另有 ${warnings.length - 60} 条`);
}
if (problems.length > 0) process.exit(1);
console.log('结论：无阻塞性错误。');
