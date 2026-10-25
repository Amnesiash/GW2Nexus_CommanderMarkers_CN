#!/usr/bin/env node
'use strict';
/**
 * 把已翻译字符串所在的字面量自动包一层 T()。
 *
 * 用法:
 *   node scripts/wrap_strings.js apply <file.cpp> [more.cpp ...]
 *   node scripts/wrap_strings.js check   <file.cpp> [...]
 *
 * 规则：
 *  - 只对「译表中存在」的完整字面量做包装；
 *  - 支持 C++ 相邻字符串字面量自动拼接（跨行也算一组）；
 *  - 跳过注释、#include 行、字符字面量和原始字符串；
 *  - 已经包过 T(...) 的不会重复包装。
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const LOCALE_DIR = path.join(ROOT, 'src', 'core', 'locale');

function loadKeys() {
  const keys = new Set();
  if (!fs.existsSync(LOCALE_DIR)) return keys;
  for (const name of fs.readdirSync(LOCALE_DIR)) {
    if (!name.endsWith('.inc')) continue;
    const src = fs.readFileSync(path.join(LOCALE_DIR, name), 'utf8');
    const re = /\{\s*"((?:[^"\\]|\\.)*)"\s*,/gs;
    let m;
    while ((m = re.exec(src))) keys.add(unescapeLiteral(m[1]));
  }
  return keys;
}

function unescapeLiteral(raw) {
  return raw.replace(/\\(.)/g, (mm, ch) => {
    const map = { n: '\n', t: '\t', r: '\r', '0': '\0', '"': '"', '\\': '\\', "'": "'", '?': '?' };
    return Object.prototype.hasOwnProperty.call(map, ch) ? map[ch] : ch;
  });
}

function escapeLiteral(s) {
  return s
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\n/g, '\\n')
    .replace(/\t/g, '\\t')
    .replace(/\r/g, '\\r');
}

/** 把注释替换成等长空格，便于保持偏移位置一一对应。 */
function blankComments(src) {
  const out = src.split('');
  let i = 0;
  const n = src.length;
  const blank = (from, to) => {
    for (let k = from; k < to; k++) {
      if (out[k] !== '\n') out[k] = ' ';
    }
  };
  while (i < n) {
    const c = src[i];
    if (c === '"') {
      if (src[i - 1] === 'R') {
        const open = src.indexOf('(', i + 1);
        if (open !== -1) {
          const delim = src.slice(i + 1, open);
          const close = src.indexOf(')' + delim + '"', open);
          i = close === -1 ? n : close + delim.length + 2;
          continue;
        }
      }
      i++;
      while (i < n) {
        if (src[i] === '\\') { i += 2; continue; }
        if (src[i] === '"') { i++; break; }
        i++;
      }
      continue;
    }
    if (c === "'") {
      i++;
      while (i < n) {
        if (src[i] === '\\') { i += 2; continue; }
        if (src[i] === "'") { i++; break; }
        i++;
      }
      continue;
    }
    if (c === '/' && src[i + 1] === '/') {
      let j = i;
      while (j < n && src[j] !== '\n') j++;
      blank(i, j);
      i = j;
      continue;
    }
    if (c === '/' && src[i + 1] === '*') {
      const end = src.indexOf('*/', i + 2);
      const j = end === -1 ? n : end + 2;
      blank(i, j);
      i = j;
      continue;
    }
    i++;
  }
  return out.join('');
}

function findLiterals(plain) {
  const lits = [];
  let i = 0;
  const n = plain.length;
  while (i < n) {
    if (plain[i] === '"') {
      const start = i;
      i++;
      while (i < n) {
        if (plain[i] === '\\') { i += 2; continue; }
        if (plain[i] === '"') { i++; break; }
        i++;
      }
      lits.push({ start, end: i, raw: plain.slice(start + 1, i - 1) });
      continue;
    }
    i++;
  }
  return lits;
}

function groupAdjacent(plain, literals) {
  const groups = [];
  for (const lit of literals) {
    const last = groups[groups.length - 1];
    if (last) {
      const between = plain.slice(last.end, lit.start);
      if (/^\s*$/.test(between)) {
        last.end = lit.end;
        last.parts.push(lit);
        continue;
      }
    }
    groups.push({ start: lit.start, end: lit.end, parts: [lit] });
  }
  return groups;
}

function isAlreadyWrapped(plain, group) {
  const m = /([A-Za-z0-9_]*)\(\s*$/.exec(plain.slice(0, group.start));
  return !!m && m[1] === 'T';
}

function lineStarts(src) {
  const starts = [0];
  for (let i = 0; i < src.length; i++) {
    if (src[i] === '\n') starts.push(i + 1);
  }
  return starts;
}

function isIncludeLine(src, starts, pos) {
  let lo = 0, hi = starts.length - 1, ans = 0;
  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    if (starts[mid] <= pos) { ans = mid; lo = mid + 1; } else hi = mid - 1;
  }
  const nl = src.indexOf('\n', starts[ans]);
  const line = src.slice(starts[ans], nl === -1 ? src.length : nl);
  return /^\s*#\s*include/.test(line);
}

function processFile(file, keys, apply) {
  const original = fs.readFileSync(file, 'utf8');
  const plain = blankComments(original);
  const starts = lineStarts(original);
  const groups = groupAdjacent(plain, findLiterals(plain));

  const edits = [];
  for (const group of groups) {
    if (isIncludeLine(original, starts, group.start)) continue;
    if (isAlreadyWrapped(plain, group)) continue;
    const joined = group.parts.map(p => unescapeLiteral(p.raw)).join('');
    if (!keys.has(joined)) continue;
    edits.push({ start: group.start, end: group.end, value: 'T("' + escapeLiteral(joined) + '")' });
  }

  if (!apply) return { count: edits.length, changed: false };

  let out = original;
  for (const e of edits.slice().sort((a, b) => b.start - a.start)) {
    out = out.slice(0, e.start) + e.value + out.slice(e.end);
  }
  if (out !== original) fs.writeFileSync(file, out, 'utf8');
  return { count: edits.length, changed: out !== original };
}

function main() {
  const args = process.argv.slice(2);
  const mode = args[0] === 'check' ? 'check' : 'apply';
  const files = args.slice(1);
  if (files.length === 0) {
    console.error('usage: node scripts/wrap_strings.js [apply|check] <file.cpp> ...');
    process.exit(2);
  }
  const keys = loadKeys();
  console.log(`loaded ${keys.size} translated keys`);
  let total = 0;
  for (const f of files) {
    const full = path.isAbsolute(f) ? f : path.join(process.cwd(), f);
    const r = processFile(full, keys, mode === 'apply');
    total += r.count;
    console.log(`${mode === 'apply' && r.changed ? 'updated' : 'ok      '} ${f}  (${r.count} literals)`);
  }
  console.log(`total: ${total}`);
}

main();
