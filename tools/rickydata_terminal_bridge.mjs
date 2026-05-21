#!/usr/bin/env node
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const DEFAULT_AGENT_IDS = [
  'cambrian-beta-lending-private-de6ec3',
  'erc8004-expert',
  'rickydatascience-copilot',
  'geo-expert',
  'rickydata-security-expert',
];

const AGENT_LABELS = {
  'cambrian-beta-lending-private-de6ec3': {
    name: 'Cambrian Beta Lending',
    description: 'Private lending agent',
  },
  'erc8004-expert': {
    name: 'ERC-8004 Expert',
    description: 'Protocol and agent wallet help',
  },
  'rickydatascience-copilot': {
    name: 'RickyDataScience Copilot',
    description: 'RickyData development copilot',
  },
  'geo-expert': {
    name: 'Geo Expert',
    description: 'Geospatial analysis assistant',
  },
  'rickydata-security-expert': {
    name: 'RickyData Security',
    description: 'Security review assistant',
  },
};

function parseArgs(argv) {
  const args = new Map();
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg.startsWith('--')) {
      const [key, inlineValue] = arg.slice(2).split('=', 2);
      if (inlineValue !== undefined) {
        args.set(key, inlineValue);
      } else if (argv[i + 1] && !argv[i + 1].startsWith('--')) {
        args.set(key, argv[++i]);
      } else {
        args.set(key, 'true');
      }
    }
  }
  return args;
}

function resolveSdkRoot() {
  if (process.env.RICKYDATA_SDK_PATH) {
    return path.resolve(process.env.RICKYDATA_SDK_PATH);
  }
  const candidates = [
    path.resolve(process.cwd(), '..', 'rickydata_SDK'),
    '/Users/riccardoesclapon/Documents/github/rickydata_SDK',
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(path.join(candidate, 'packages/core/dist/agent/index.js'))) {
      return candidate;
    }
  }
  return candidates[0];
}

async function loadAgentClient() {
  const sdkRoot = resolveSdkRoot();
  const modulePath = path.join(sdkRoot, 'packages/core/dist/agent/index.js');
  if (!fs.existsSync(modulePath)) {
    throw new Error(`RickyData SDK build not found at ${modulePath}. Run npm run build in ${sdkRoot}.`);
  }
  const mod = await import(pathToFileURL(modulePath).href);
  return { AgentClient: mod.AgentClient, sdkRoot };
}

function readLocalProfile(profileName) {
  const credentialsPath = path.join(os.homedir(), '.rickydata', 'credentials.json');
  const credentials = JSON.parse(fs.readFileSync(credentialsPath, 'utf8'));
  const profile = credentials.profiles?.[profileName];
  if (!profile?.token) {
    throw new Error(`No RickyData token found for profile "${profileName}" in ${credentialsPath}`);
  }
  return {
    token: profile.token,
    walletAddress: profile.walletAddress ?? null,
  };
}

function sanitizeError(error, token) {
  const message = error?.stack || error?.message || String(error);
  return token ? message.replaceAll(token, '<wallet-token>') : message;
}

function readJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => {
      body += chunk;
      if (body.length > 64 * 1024) {
        req.destroy();
        reject(new Error('Request body too large'));
      }
    });
    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch (error) {
        reject(error);
      }
    });
    req.on('error', reject);
  });
}

function writeJson(res, status, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    'Content-Type': 'application/json',
    'Cache-Control': 'no-store',
    'Content-Length': Buffer.byteLength(body),
    Connection: 'close',
  });
  res.end(body);
}

function writeSse(res, event, text) {
  const encoded = Buffer.from(String(text ?? ''), 'utf8').toString('base64');
  res.write(`event: ${event}\n`);
  res.write(`data: ${encoded}\n\n`);
}

async function listSelectedAgents(client) {
  const allAgents = await client.listAgents();
  const byId = new Map(allAgents.map((agent) => [agent.id, agent]));

  return DEFAULT_AGENT_IDS.map((id) => {
    const live = byId.get(id);
    const fallback = AGENT_LABELS[id] ?? { name: id, description: '' };
    return {
      id,
      name: live?.name || live?.title || fallback.name,
      description: live?.description || fallback.description,
      available: Boolean(live),
    };
  });
}

function truncateForLvgl(value, max = 72) {
  const text = String(value || '').replace(/\s+/g, ' ').trim();
  if (text.length <= max) return text;
  return `${text.slice(0, Math.max(0, max - 3))}...`;
}

async function main() {
  const args = parseArgs(process.argv);
  const port = Number(args.get('port') || process.env.RICKYDATA_BRIDGE_PORT || 8765);
  const host = args.get('host') || process.env.RICKYDATA_BRIDGE_HOST || '127.0.0.1';
  const profileName = args.get('profile') || process.env.RICKYDATA_PROFILE || 'default';
  const gatewayUrl = args.get('gateway') || process.env.AGENT_GATEWAY_URL || 'https://agents.rickydata.org';

  const { AgentClient, sdkRoot } = await loadAgentClient();
  const profile = readLocalProfile(profileName);
  const client = new AgentClient({ token: profile.token, gatewayUrl });

  const server = http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url ?? '/', `http://${req.headers.host ?? `${host}:${port}`}`);

      if (req.method === 'GET' && url.pathname === '/health') {
        writeJson(res, 200, {
          ok: true,
          profile: profileName,
          walletAddress: profile.walletAddress,
          gatewayUrl,
          sdkRoot,
        });
        return;
      }

      if (req.method === 'GET' && url.pathname === '/agents') {
        writeJson(res, 200, {
          agents: await listSelectedAgents(client),
        });
        return;
      }

      if (req.method === 'GET' && url.pathname === '/agents-lvgl') {
        const agents = await listSelectedAgents(client);
        writeJson(res, 200, agents.map((agent) => ({
          id: agent.id,
          name: agent.name,
          description: agent.available
            ? truncateForLvgl(agent.description)
            : 'Unavailable for this wallet/profile',
        })));
        return;
      }

      if (req.method === 'POST' && url.pathname === '/chat') {
        const body = await readJsonBody(req);
        const agentId = String(body.agentId || '');
        const message = String(body.message || '');
        const model = body.model ? String(body.model) : (process.env.RICKYDATA_MODEL || undefined);
        if (!agentId || !message) {
          writeJson(res, 400, { error: 'agentId and message are required' });
          return;
        }

        res.writeHead(200, {
          'Content-Type': 'text/event-stream',
          'Cache-Control': 'no-cache, no-transform',
          Connection: 'close',
          'X-Accel-Buffering': 'no',
        });
        writeSse(res, 'status', `SDK bridge connected (${profileName}).`);
        writeSse(res, 'status', `Sending to ${agentId}.`);

        try {
          const result = await client.chat(agentId, message, {
            model,
            onText: (text) => writeSse(res, 'text', text),
            onToolCall: (tool) => writeSse(res, 'tool_call', tool.displayName || tool.name || 'tool'),
            onToolResult: (tool) => {
              const summary = typeof tool.result === 'string'
                ? tool.result.slice(0, 240)
                : JSON.stringify(tool.result ?? {}).slice(0, 240);
              writeSse(res, 'tool_result', summary);
            },
          });
          const cost = result?.cost ? ` cost=${result.cost}` : '';
          const tools = typeof result?.toolCallCount === 'number' ? ` tools=${result.toolCallCount}` : '';
          writeSse(res, 'done', `Done.${cost}${tools}`);
        } catch (error) {
          writeSse(res, 'error', sanitizeError(error, profile.token));
        } finally {
          res.end();
        }
        return;
      }

      writeJson(res, 404, { error: 'not found' });
    } catch (error) {
      writeJson(res, 500, { error: sanitizeError(error, profile.token) });
    }
  });

  server.listen(port, host, () => {
    console.log(`RickyData terminal bridge listening at http://${host}:${port}`);
    console.log(`Profile: ${profileName}`);
    console.log(`Wallet: ${profile.walletAddress ?? '(unknown)'}`);
    console.log(`Gateway: ${gatewayUrl}`);
    console.log(`SDK: ${sdkRoot}`);
  });
}

main().catch((error) => {
  console.error(error?.stack || error);
  process.exit(1);
});
