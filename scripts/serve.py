#!/usr/bin/env python3
"""Serve only the public STI2D-OS portal assets and downloads.

This is a convenience server, not a production-grade hosting stack. It deliberately
refuses access to firmware sources, Epsilon source, docs, tests, messaging internals,
configuration files and other project directories. Browser-delivered portal code is
still inspectable by design; keep secrets and proprietary server logic off the client.
"""
import http.server
import os
import posixpath
import socket
import socketserver
import sys
from urllib.parse import unquote, urlsplit

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


def allowed(path):
    path = posixpath.normpath('/' + path.lstrip('/'))
    parts = [x for x in path.split('/') if x]
    if not parts or any(x.startswith('.') for x in parts):
        return False
    if parts[0] == 'web':
        return True  # Frontend files must be delivered to the browser to run.
    if parts[0] == 'library' and len(parts) == 3 and parts[1] in ('games', 'ai'):
        return parts[2].lower().endswith('.nwa')
    if parts[0] == 'release' and len(parts) == 3 and parts[1] in ('n0110', 'n0115', 'n0120'):
        return parts[2] == 'STI2D-OS.nwa'
    if parts[0] == 'userland' and len(parts) == 3 and parts[1] == 'firmware':
        return False
    if parts[0] == 'userland' and len(parts) == 4 and parts[1] == 'firmware' and parts[2] in ('n0110', 'n0115', 'n0120'):
        return parts[3].endswith('.dfu')
    if parts == ['installer', 'Installer-STI2D-OS-V2.html']:
        return True
    return False


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = dict(http.server.SimpleHTTPRequestHandler.extensions_map)
    extensions_map.update({'.webmanifest': 'application/manifest+json', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.nwa': 'application/octet-stream', '.dfu': 'application/octet-stream', '.py': 'application/octet-stream'})

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    def list_directory(self, path):
        self.send_error(403, 'Directory listing disabled')
        return None

    def do_GET(self):
        if urlsplit(self.path).path == '/':
            self.send_response(302)
            self.send_header('Location', '/web/')
            self.end_headers()
            return
        super().do_GET()

    def do_HEAD(self):
        if urlsplit(self.path).path == '/':
            self.send_response(302)
            self.send_header('Location', '/web/')
            self.end_headers()
            return
        super().do_HEAD()

    def translate_path(self, path):
        raw = unquote(urlsplit(path).path)
        clean = posixpath.normpath('/' + raw.lstrip('/'))
        if clean == '/':
            return os.path.join(ROOT, 'web', 'index.html')
        if not allowed(clean):
            return os.path.join(ROOT, '__sti2d_forbidden__')
        return os.path.join(ROOT, *[x for x in clean.split('/') if x])

    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Referrer-Policy', 'strict-origin-when-cross-origin')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('Permissions-Policy', 'camera=(), microphone=(), geolocation=()')
        super().end_headers()

    def log_message(self, fmt, *args):
        pass


def lan_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('10.255.255.255', 1))
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


if __name__ == '__main__':
    with Server(('0.0.0.0', PORT), Handler) as httpd:
        print('STI2D-OS : portail en ligne (serveur de développement)')
        print(f'  Sur cet ordinateur : http://localhost:{PORT}/web/')
        ip = lan_address()
        if ip:
            print(f'  Sur un téléphone du même Wi-Fi : http://{ip}:{PORT}/web/')
        print('  Accès limité aux fichiers publics nécessaires au portail.')
        print('  Pour une mise en ligne publique, utiliser un hébergement HTTPS avec configuration serveur adaptée.')
        print('Ctrl+C pour arrêter.')
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass
