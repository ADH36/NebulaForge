const MAX_QUERY_LENGTH = 512;
const MAX_PAGE_LENGTH = 2_000_000;

function requiredString(args: Record<string, unknown>, key: string): string {
  const value = args[key];
  if (typeof value !== 'string' || value.trim().length === 0) throw new Error(`${key} is required`);
  if (value.length > MAX_QUERY_LENGTH) throw new Error(`${key} exceeds ${MAX_QUERY_LENGTH} characters`);
  return value.trim();
}

function safeUrl(value: string): string {
  const url = new URL(value);
  if (url.protocol !== 'http:' && url.protocol !== 'https:') throw new Error('Only http and https URLs are supported');
  return url.toString();
}

async function getText(url: string, timeoutMs = 30_000): Promise<{ status: number; text: string }> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { signal: controller.signal, headers: { 'User-Agent': 'NebulaForge/0.5 research tool' } });
    const text = await response.text();
    return { status: response.status, text: text.slice(0, MAX_PAGE_LENGTH) };
  } finally {
    clearTimeout(timer);
  }
}

async function postJson(url: string, body: Record<string, unknown>, apiKey: string, timeoutMs = 45_000): Promise<{ status: number; contentType: string; bytes: Uint8Array }> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, {
      method: 'POST',
      signal: controller.signal,
      headers: { 'Content-Type': 'application/json', 'X-API-Key': apiKey, 'User-Agent': 'NebulaForge/0.5 terrain tool' },
      body: JSON.stringify(body)
    });
    return { status: response.status, contentType: response.headers.get('content-type') ?? '', bytes: new Uint8Array(await response.arrayBuffer()) };
  } finally {
    clearTimeout(timer);
  }
}

function htmlToText(html: string): string {
  return html.replace(/<script[\s\S]*?<\/script>/gi, '').replace(/<style[\s\S]*?<\/style>/gi, '')
    .replace(/<[^>]+>/g, ' ').replace(/&nbsp;/gi, ' ').replace(/&amp;/gi, '&')
    .replace(/\s+/g, ' ').trim();
}

export async function handleResearchTools(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  const action = typeof args.action === 'string' ? args.action.trim().toLowerCase() : '';
  if (!action) return { success: false, error: 'MISSING_ACTION', message: 'action is required' };

  try {
    if (action === 'search') {
      const query = requiredString(args, 'query');
      const response = await getText(`https://lite.duckduckgo.com/lite/?q=${encodeURIComponent(query)}`, 20_000);
      if (response.status < 200 || response.status >= 300) return { success: false, error: `HTTP_${response.status}`, query };
      const results: Array<Record<string, string>> = [];
      const linkPattern = /<a[^>]+href=["']([^"']+)["'][^>]*>([\s\S]*?)<\/a>/gi;
      for (const match of response.text.matchAll(linkPattern)) {
        const href = match[1];
        const title = htmlToText(match[2]);
        if (!title || !href || href.includes('duckduckgo.com')) continue;
        results.push({ title, url: safeUrl(href) });
        if (results.length >= 15) break;
      }
      return { success: true, query, result_count: results.length, results };
    }

    if (action === 'fetch_page') {
      const pageUrl = safeUrl(requiredString(args, 'url'));
      const response = await getText(`https://r.jina.ai/${pageUrl}`, 45_000);
      if (response.status < 200 || response.status >= 300) return { success: false, error: `HTTP_${response.status}`, url: pageUrl };
      return { success: true, url: pageUrl, content: response.text };
    }

    if (action === 'geocode' || action === 'reverse_geocode') {
      const url = action === 'geocode'
        ? `https://nominatim.openstreetmap.org/search?format=json&limit=5&addressdetails=1&q=${encodeURIComponent(requiredString(args, 'query'))}`
        : `https://nominatim.openstreetmap.org/reverse?format=json&addressdetails=1&lat=${encodeURIComponent(requiredString(args, 'lat'))}&lon=${encodeURIComponent(requiredString(args, 'lng'))}`;
      const response = await getText(url, 20_000);
      if (response.status < 200 || response.status >= 300) return { success: false, error: `HTTP_${response.status}` };
      const data: unknown = JSON.parse(response.text);
      if (action === 'reverse_geocode' && typeof data === 'object' && data !== null) {
        const result = data as Record<string, unknown>;
        if (typeof result.lon === 'string') result.lng = result.lon;
        return { success: true, ...result };
      }
      return { success: true, query: args.query, results: data };
    }

    return { success: false, error: 'UNKNOWN_ACTION', message: `Unsupported research action: ${action}` };
  } catch (error) {
    return { success: false, error: 'RESEARCH_FAILED', message: error instanceof Error ? error.message : 'Research request failed' };
  }
}

export async function handleTerrainData(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  const action = typeof args.action === 'string' ? args.action.trim().toLowerCase() : '';
  const validActions = ['generate_heightmap', 'preview_elevation', 'get_map_image', 'list_styles', 'get_water_features'];
  if (!validActions.includes(action)) return { success: false, error: 'UNKNOWN_ACTION', message: `Unsupported terrain action: ${action}` };

  const apiKey = process.env.MCP_TERRAIN_API_KEY;
  if (!apiKey) return { success: false, error: 'TERRAIN_API_KEY_REQUIRED', message: 'Set MCP_TERRAIN_API_KEY to use real-world terrain data.' };
  const baseUrl = process.env.MCP_TERRAIN_API_BASE_URL ?? 'https://www.openstreetmap.org';
  const endpoint = action === 'generate_heightmap' ? 'heightmap' : action === 'preview_elevation' ? 'preview' : action === 'get_map_image' ? 'map-image' : action === 'get_water_features' ? 'water-features' : 'styles';
  try {
    if (action === 'list_styles') {
      const response = await getText(`${baseUrl}/api/terrain/styles`, 20_000);
      if (response.status < 200 || response.status >= 300) return { success: false, error: `HTTP_${response.status}` };
      return { success: true, styles: JSON.parse(response.text) };
    }
    const body = { ...args };
    delete body.action;
    const response = await postJson(`${baseUrl}/api/terrain/${endpoint}`, body, apiKey);
    if (response.status < 200 || response.status >= 300) return { success: false, error: `HTTP_${response.status}`, status: response.status };
    const text = new TextDecoder().decode(response.bytes);
    if (response.contentType.includes('json') || text.trimStart().startsWith('{') || text.trimStart().startsWith('[')) {
      return { success: true, ...((JSON.parse(text) as Record<string, unknown>)) };
    }
    return { success: true, action, contentType: response.contentType, dataBase64: Buffer.from(response.bytes).toString('base64'), sizeBytes: response.bytes.byteLength };
  } catch (error) {
    return { success: false, error: 'TERRAIN_REQUEST_FAILED', message: error instanceof Error ? error.message : 'Terrain request failed' };
  }
}
