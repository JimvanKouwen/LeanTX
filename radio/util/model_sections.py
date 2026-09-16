#!/usr/bin/env python3
"""Generate explicit section switches from semantic model fields (never C++ layout)."""
from pathlib import Path
import re
root_dir = Path(__file__).resolve().parents[2]
src=(root_dir / 'radio/src/storage/model_config_fields.inc').read_text()
rows=[]
for l in src.splitlines():
 m=re.match(r'(\w+)\("([^"]+)", (\d+),',l)
 if not m: continue
 macro,path,count=m.groups(); stride,inner={'MSCRIPT2':(6,1),'MSENSOR2':(4,1),'MMONO4':(4,1),'MMONO8':(8,1),'MMONOLINES':(12,3),'MCOLOR10':(10,1),'MCOLOR500':(500,50),'MCOLOR50':(50,1)}.get(macro,(1,1))
 rows.append((path,int(count),stride,inner))
class Node:
 def __init__(self,path=''): self.path=path; self.children={}; self.row=None; self.id=0; self.extent=0
root=Node()
for r,(path,count,stride,inner) in enumerate(rows):
 node=root; dim=0
 for part in path.split('/'):
  if part not in node.children: node.children[part]=Node(node.path+'/'+part)
  node=node.children[part]
  if part=='%u': node.extent=max(node.extent,[count//stride,stride//inner,inner][dim]); dim+=1
 node.row=r
nodes=[]
def number(n):
 n.id=len(nodes); nodes.append(n)
 for c in n.children.values(): number(c)
number(root)
lines=['// Explicit section dispatch for model_config_fields.inc. GPL-2.0-or-later.', '// Regenerate with radio/util/model_sections.py. No runtime path matching.', 'void enter(void* ctx, const Node& parent, const char* key, const char* text, Node& child, Result& result)', '{','  child = parent; child.section = -1;', '  switch (parent.section) {']
checks = ['static_assert(DescriptorCount == %d, "regenerate model_config_sections.inc");' % len(rows)]
for r,(path,count,stride,inner) in enumerate(rows):
 checks.append(f'static_assert(sameKey(descriptors[{r}].path, "{path}") && descriptors[{r}].count == {count} && descriptors[{r}].stride == {stride} && descriptors[{r}].innerStride == {inner}, "regenerate model_config_sections.inc");')
lines[2:2] = checks
def local_index(row, prefix):
 path,count,stride,inner=rows[row]
 dims=path.count('%u')
 return ' + '.join(f'{prefix}[{d}] * {factor}' for d,factor in enumerate((stride,inner,1)[:dims])) or '0'
def action(n):
 out=[]
 if n.children: out.append(f'child.section = {n.id};')
 r=n.row
 # numeric scalar list alias
 if r is None and list(n.children)==['val']: r=n.children['val'].row
 if r is not None: out.append(("if (*text) " if n.children else "") + f'readLeaf(ctx, {r}, {local_index(r, "child.index")}, text, result);')
 elif n.children: out.append('if (*text) result.error = "expected configuration mapping";')
 return ' '.join(out)
for n in nodes:
 if not n.children: continue
 lines.append(f'    case {n.id}: // {n.path or "root"}')
 for key,c in n.children.items():
  if key=='%u':
   dim=c.path.count('%u')-1
   if n.path=='/switchWarning': lines += ['      { int64_t i; int physical = switchLookupIdx(key, strlen(key));', f'        if (physical >= 0) i = physical; else if (!integer(key, 0, {c.extent-1}, i)) break;']
   else: lines += [f'      {{ int64_t i; if (!integer(key, 0, {c.extent-1}, i)) break;']
   lines += [f'        child.index[{dim}] = i; {action(c)} return; }}']
  else: lines.append(f'      if (!strcmp(key, "{key}")) {{ {action(c)} return; }}')
 lines.append('      break;')
lines += ['  }', '  if (*text) ++result.unknown;', '}','void writeSection(Context& context, unsigned section, unsigned* index, Writer& writer, Field& field)', '{','  switch (section) {']
for n in nodes:
 if not n.children: continue
 lines.append(f'    case {n.id}: // {n.path or "root"}')
 for key,c in n.children.items():
  if key=='%u':
   dim=c.path.count('%u')-1
   lines.append(f'      for (index[{dim}] = 0; index[{dim}] < {c.extent}; ++index[{dim}]) {{')
   lines.append(f'        if (!used(context, "{c.path[1:]}", index)) continue;')
   begin=f'writer.begin(index[{dim}]);'
   if n.path=='/switchWarning': begin='writer.begin(switchGetDefaultName(index[0]));'
   lines.append('        '+begin)
  elif c.children: lines.append(f'      writer.begin("{key}");')
  if c.children: lines.append(f'      writeSection(context, {c.id}, index, writer, field);')
  elif c.row is not None: lines.append(f'      writeLeaf(context, {c.row}, {local_index(c.row, "index")}, "{key}", writer, field);')
  if c.children or key=='%u': lines.append('      writer.end();')
  if key=='%u': lines.append('      }')
 lines.append('      break;')
lines += ['  }','}']
(root_dir / 'radio/src/storage/model_config_sections.inc').write_text('\n'.join(lines)+'\n')
