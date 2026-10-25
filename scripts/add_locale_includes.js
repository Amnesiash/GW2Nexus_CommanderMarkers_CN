#!/usr/bin/env node
'use strict';
/**
 * 给使用了 T()/Ts() 的源文件补齐本地化基础设施：
 *   #include "core/Locale.h"
 *   using cm::Locale::T;  (以及需要时的 Ts)
 * 放在该文件第一个匿名 namespace 的开头（与既有写法保持一致）。
 *
 * 用法: node scripts/add_locale_includes.js
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const SRC_DIR = path.join(ROOT, 'src');

const files = [];
const walk = (d) => {
  for (const name of fs.readdirSync(d)) {
    const full = path.join(d, name);
    if (fs.statSync(full).isDirectory()) { walk(full); continue; }
    if (/\.(cpp|h)$/.test(name)) files.push(full);
  }
};
walk(SRC_DIR);

function stripComments(src) {
  return src.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, '');
}

let changed = 0;
for (const full of files) {
  const rel = path.relative(ROOT, full).replace(/\\/g, '/');
  if (/src[\\/]core[\\/]Locale\.(cpp|h)$/.test(full)) continue;
  let src = fs.readFileSync(full, 'utf8');
  const plain = stripComments(src);
  if (!/\bTs?\s*\(/.test(plain)) continue;

  const needsT = /\bT\s*\(/.test(plain);
  const needsTs = /\bTs\s*\(/.test(plain);
  const hasInclude = /#include\s+"core\/Locale\.h"/.test(plain);
  const hasUsingT = /using\s+cm::Locale::T\s*;/.test(plain);
  const hasUsingTs = /using\s+cm::Locale::Ts\s*;/.test(plain);
  if (hasInclude && (!needsT || hasUsingT) && (!needsTs || hasUsingTs)) continue;

  // 1) include：插到最后一个 #include "..." 之后，保持分组
  if (!hasInclude) {
    const incRe = /^[ \t]*#include\s+"[^"]+"[ \t]*\r?\n/gm;
    let last = null, m;
    while ((m = incRe.exec(src)) !== null) last = m;
    if (last) {
      const at = last.index + last[0].length;
      src = src.slice(0, at) + '#include "core/Locale.h"\n' + src.slice(at);
    } else {
      const first = /^[ \t]*#include\s+<[^>]+>[ \t]*\r?\n/m.exec(src);
      const at = first ? first.index : 0;
      src = src.slice(0, at) + '#include "core/Locale.h"\n\n' + src.slice(at);
    }
  }

  // 2) using：放到第一个 `namespace {` 之后（该文件内）
  const nsRe = /\bnamespace\s*\{/;
  const ns = nsRe.exec(src);
  if (ns) {
    const at = ns.index + ns[0].length;
    let block = '\n\n// Short alias so the wrapped literals stay readable.\n';
    if (needsT && !hasUsingT) block += 'using cm::Locale::T;\n';
    if (needsTs && !hasUsingTs) block += 'using cm::Locale::Ts;\n';
    src = src.slice(0, at) + block + src.slice(at);
  } else {
    console.warn(`  [skip] ${rel}: 找不到匿名 namespace，需手工补 using`);
    continue;
  }

  fs.writeFileSync(full, src, 'utf8');
  console.log(`  updated ${rel}`);
  changed++;
}

console.log(`\n完成：更新 ${changed} 个文件。`);
