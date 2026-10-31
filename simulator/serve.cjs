const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const port = Number(process.argv[2] || 8080);
if (!Number.isInteger(port) || port < 1024 || port > 65535) {
    throw new Error("Port must be an integer from 1024 to 65535.");
}
const root = path.join(__dirname, "build");
const files = new Map([
    ["/", ["index.html", "text/html"]],
    ["/index.html", ["index.html", "text/html"]],
    ["/preview.js", ["preview.js", "text/javascript"]],
    ["/test.js", ["test.js", "text/javascript"]],
    ["/hourglass.js", ["hourglass.js", "text/javascript"]],
    ["/hourglass.wasm", ["hourglass.wasm", "application/wasm"]],
]);
if (!fs.existsSync(path.join(root, "hourglass.wasm"))) {
    throw new Error("Build the simulator before starting the server.");
}
const server = http.createServer((request, response) => {
    const url = new URL(request.url, `http://127.0.0.1:${port}`);
    const file = files.get(url.pathname);
    if (!file || !["GET", "HEAD"].includes(request.method)) {
        response.writeHead(404).end("Not found");
        return;
    }
    fs.readFile(path.join(root, file[0]), (error, data) => {
        if (error) {
            console.error(error);
            response.writeHead(500).end("Preview file could not be read");
            return;
        }
        response.writeHead(200, {
            "Content-Type": file[1],
            "Cache-Control": "no-store",
            "X-Content-Type-Options": "nosniff",
        });
        response.end(request.method === "HEAD" ? undefined : data);
    });
});
server.on("error", (error) => {
    console.error(`Preview server failed: ${error.message}`);
    process.exitCode = 1;
});
server.listen(port, "127.0.0.1", () => {
    console.log(`Hourglass preview: http://127.0.0.1:${port}`);
});
