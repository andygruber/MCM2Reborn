// A release's notes to Discord, in MCM2:Reborn's colours: node announce-release.mjs [tag]
// (the latest release without one). The webhooks are the DISCORD_WEBHOOK_URL and
// DISCORD_CHANGELOG_WEBHOOK_URL secrets; GITHUB_REPOSITORY and GITHUB_TOKEN as Actions sets them.
// The changelog channel's post pings @everyone. DRY_RUN=1 prints the messages instead.

const repo = process.env.GITHUB_REPOSITORY || "xshxdex98/MCM2Reborn";
const webhooks = [
    { url: process.env.DISCORD_WEBHOOK_URL, ping: false },
    { url: process.env.DISCORD_CHANGELOG_WEBHOOK_URL, ping: true },
].filter(w => w.url);
const tag = process.argv[2] || process.env.RELEASE_TAG || "";

const ORANGE = 0xFF8C1A;
const LOGO = `https://raw.githubusercontent.com/${repo}/main/assets/logo.png`;
const EMBED_TEXT = 4000, MESSAGE_TEXT = 5500;      // Discord's limits are 4096 and 6000

async function release() {
    const headers = { accept: "application/vnd.github+json" };
    if (process.env.GITHUB_TOKEN)
        headers.authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
    const path = tag ? `tags/${encodeURIComponent(tag)}` : "latest";
    const answer = await fetch(`https://api.github.com/repos/${repo}/releases/${path}`, { headers });
    if (!answer.ok)
        throw new Error(`GitHub: ${answer.status} ${await answer.text()}`);
    return answer.json();
}

// GitHub's markdown as Discord shows it in an embed: headings bold, bullets as dots; a heading
// first is the release's title again, and goes.
function discordText(notes) {
    return notes.replace(/\r/g, "").trim().replace(/^#{1,6}\s+.*\n/, "").split("\n").map(line => {
        const heading = line.match(/^#{1,6}\s+(.*)$/);
        if (heading)
            return `\n**${heading[1].trim()}**`;
        return line.replace(/^(\s*)[-*]\s+/, "$1• ");
    }).join("\n").replace(/\n{3,}/g, "\n\n").trim();
}

// Whole lines into pieces of at most `most` characters.
function pieces(text, most) {
    const out = [];
    let piece = "";
    for (const line of text.split("\n")) {
        for (let rest = line; ; ) {
            const room = most - piece.length - (piece ? 1 : 0);
            if (rest.length <= room) {
                piece += (piece ? "\n" : "") + rest;
                break;
            }
            if (piece) {
                out.push(piece);
                piece = "";
                continue;
            }
            out.push(rest.slice(0, most));
            rest = rest.slice(most);
        }
    }
    if (piece)
        out.push(piece);
    return out;
}

function embedsFor(r) {
    const version = r.tag_name.replace(/^v/, "");
    const download = r.assets.find(a => /launcher.*\.exe$/i.test(a.name)) || r.assets.find(a => /\.zip$/i.test(a.name));
    const parts = pieces(discordText(r.body || "The notes are on GitHub."), EMBED_TEXT);
    const embeds = parts.map(text => ({ color: ORANGE, description: text }));

    Object.assign(embeds[0], {
        author: { name: "MCM2:REBORN  ·  UPDATE", url: `https://github.com/${repo}`, icon_url: LOGO },
        title: r.name && r.name !== r.tag_name ? r.name : `Version ${version} is out`,
        url: r.html_url,
        thumbnail: { url: LOGO },
    });
    Object.assign(embeds[embeds.length - 1], {
        fields: [
            { name: "Updating", value: "Start the MCM2:Reborn launcher: it downloads the update for you.", inline: false },
            ...(download ? [{ name: "New here?", value: `[${download.name}](${download.browser_download_url})`, inline: true }] : []),
            { name: "Release", value: `[${r.tag_name} on GitHub](${r.html_url})`, inline: true },
        ],
        footer: { text: "Motocross Madness 2  ·  Reborn" },
        timestamp: r.published_at,
    });
    return embeds;
}

// Embeds into messages within Discord's limits: ten a message, 6000 characters in all.
function messages(embeds) {
    const out = [];
    let message = [], size = 0;
    for (const embed of embeds) {
        const length = JSON.stringify(embed).length;
        if (message.length && (message.length === 10 || size + length > MESSAGE_TEXT)) {
            out.push(message);
            message = [];
            size = 0;
        }
        message.push(embed);
        size += length;
    }
    if (message.length)
        out.push(message);
    return out;
}

// The first message pings @everyone when `ping` is on.
function bodies(embeds, ping) {
    return messages(embeds).map((batch, i) => ({
        username: "MCM2:Reborn", avatar_url: LOGO, embeds: batch,
        ...(ping && i === 0 ? { content: "@everyone", allowed_mentions: { parse: ["everyone"] } }
                            : { allowed_mentions: { parse: [] } }),
    }));
}

async function post(webhook, embeds) {
    for (const body of bodies(embeds, webhook.ping)) {
        const answer = await fetch(webhook.url, {
            method: "POST",
            headers: { "content-type": "application/json" },
            body: JSON.stringify(body),
        });
        if (!answer.ok)
            throw new Error(`Discord: ${answer.status} ${await answer.text()}`);
        await new Promise(done => setTimeout(done, 700));
    }
}

if (!webhooks.length && !process.env.DRY_RUN)
    throw new Error("Add DISCORD_WEBHOOK_URL or DISCORD_CHANGELOG_WEBHOOK_URL as a repository secret.");
const r = await release();
if (process.env.DRY_RUN)
    console.log(JSON.stringify(bodies(embedsFor(r), true), null, 1));
else if (r.draft || r.prerelease)
    console.log(`${r.tag_name} is a draft or pre-release: not announced`);
else
    for (const webhook of webhooks)
        await post(webhook, embedsFor(r));
