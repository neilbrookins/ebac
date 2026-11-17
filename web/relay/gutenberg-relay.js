/*
 * EBaC's relay to Project Gutenberg, a Cloudflare Worker (see web/README.md).
 *
 * A web page may read what it downloads from another site only if that
 * site's answer says so (the header Access-Control-Allow-Origin: CORS,
 * which the browser enforces). GitHub's answers say so; Project
 * Gutenberg's don't, neither www.gutenberg.org's nor its mirror's. So
 * EBaC's web page asks this relay instead, which downloads the same file
 * from Gutenberg, as any program can, and passes it on unchanged, with that
 * header added, for EBaC's page only.
 *
 * Only what EBaC's importer downloads (gutenberg.c), from these two:
 *   /www.gutenberg.org/ebooks/N             an ebook's page (its title,
 *                                           last update, copyright status)
 *   /www.gutenberg.org/ebooks/N.txt.utf-8   its plain text
 *   /gutenberg.pglaf.org/cache/epub/N/pgN.txt   the same text, from
 *                                           Gutenberg's mirror
 * GET and HEAD only (HEAD: its version, without the file). Anything else
 * is refused, so that it is no open proxy.
 */

// The pages that may read through it: EBaC's web page; and, for its tests,
// one served from this computer (web/test.mjs).
const PAGES = ["https://neilbrookins.github.io"];
const LOCAL = /^http:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/;

const ALLOWED = [
        /^\/www\.gutenberg\.org\/ebooks\/\d+$/,
        /^\/www\.gutenberg\.org\/ebooks\/\d+\.txt\.utf-8$/,
        /^\/gutenberg\.pglaf\.org\/cache\/epub\/(\d+)\/pg\1\.txt$/,
];

export default {
        async fetch(request) {
                const origin = request.headers.get("Origin") || "";
                const allowed = PAGES.includes(origin) || LOCAL.test(origin);
                const cors = {
                        "Access-Control-Allow-Origin": allowed ? origin : PAGES[0],
                        // (what the importer reads of a HEAD: its version)
                        "Access-Control-Expose-Headers": "ETag, Content-Length, Last-Modified",
                        "Vary": "Origin",
                };
                if (request.method === "OPTIONS")
                        return new Response(null, {
                                status: 204,
                                headers: { ...cors, "Access-Control-Allow-Methods": "GET, HEAD" },
                        });
                if (request.method !== "GET" && request.method !== "HEAD")
                        return new Response("GET or HEAD only\n", { status: 405, headers: cors });
                const path = new URL(request.url).pathname;
                if (!ALLOWED.some((re) => re.test(path)))
                        return new Response("Not one of Project Gutenberg's ebooks, as EBaC asks for them\n",
                                            { status: 404, headers: cors });
                let answer;
                try {
                        answer = await fetch("https:/" + path, {
                                method: request.method,
                                redirect: "follow",
                                headers: { "User-Agent": "EBaC web page relay (https://github.com/neilbrookins/ebac)" },
                        });
                } catch (e) {
                        return new Response("Couldn't reach " + path.split("/")[1] + "\n", { status: 502, headers: cors });
                }
                const headers = new Headers();
                // (Content-Length of a HEAD only: a file's is the runtime's own
                // to give, as it may come compressed)
                const names = ["Content-Type", "ETag", "Last-Modified"];
                if (request.method === "HEAD")
                        names.push("Content-Length");
                for (const name of names) {
                        const v = answer.headers.get(name);
                        if (v !== null)
                                headers.set(name, v);
                }
                for (const [name, v] of Object.entries(cors))
                        headers.set(name, v);
                return new Response(request.method === "HEAD" ? null : answer.body,
                                    { status: answer.status, headers });
        },
};
