import { writeFile } from 'node:fs/promises';
import { fetchProduct } from './product-source.ts';

const input = process.argv[2];
const output = process.argv[3];
if (!input) throw new Error('Usage: npm run product:inspect -- <FLYMEe URL> [output.json]');
const product = await fetchProduct(input);
const json = JSON.stringify(product, null, 2);
if (output) await writeFile(output, json, { encoding: 'utf8', flag: 'wx' });
else process.stdout.write(json + '\n');
