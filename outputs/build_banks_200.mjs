import fs from 'node:fs/promises';
import assert from 'node:assert/strict';
import {Workbook} from '@oai/artifact-tool';
const data=JSON.parse(await fs.readFile(new URL('./banks_200_data.json',import.meta.url),'utf8'));
const dir=new URL('./Bancos_200/',import.meta.url);await fs.mkdir(dir,{recursive:true});
for(const [name,rows] of Object.entries(data)){
 const wb=Workbook.create();const s=wb.worksheets.add(name);s.getRange('A1:B200').values=rows;wb.recalculate();
 assert.deepEqual(s.getRange('A1:B200').values,rows);
 const text='\uFEFF'+rows.map(r=>r.join(',')).join('\r\n')+'\r\n';
 await fs.writeFile(new URL(`WB_${name}_200.csv`,dir),text,'utf8');
 console.log(name,rows.length,(await wb.inspect({kind:'table',range:`${name}!A1:B3`,tableMaxRows:3,tableMaxCols:2})).ndjson);
}
