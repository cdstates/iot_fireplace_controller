import http.server
import socketserver
import json
import time
import urllib.parse
import os

# Ensure we are serving files from the directory where this script is located
os.chdir(os.path.dirname(os.path.abspath(__file__)))

PORT = 8000

# Mock State
state = {
    "active": False,
    "safety": True,
    "thermo_mv": 350,
    "wall_switch": False,
    "valve_state": 0,
    "led_state": 0,
    "bypass_state": 0,
    "timer_left": 0,
    "timer_end_time": 0
}

class MockHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed_path = urllib.parse.urlparse(self.path)
        
        # Serve the main page
        if parsed_path.path == "/":
            self.path = "index.html"
            return http.server.SimpleHTTPRequestHandler.do_GET(self)
            
        # Mock /status endpoint
        elif parsed_path.path == "/status":
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            
            # Update timer logic
            if state["timer_end_time"] > 0:
                now = time.time()
                if now < state["timer_end_time"]:
                    state["timer_left"] = int(state["timer_end_time"] - now)
                else:
                    state["timer_left"] = 0
                    state["timer_end_time"] = 0
                    state["active"] = False
            
            # Update GPIO mocks based on active state
            if state["active"] and state["safety"]:
                state["valve_state"] = 1
                state["led_state"] = 1
            else:
                state["valve_state"] = 0
                state["led_state"] = 0
                
            self.wfile.write(json.dumps(state).encode())
            
        # Mock /control endpoint
        elif parsed_path.path == "/control":
            query = urllib.parse.parse_qs(parsed_path.query)
            
            if 'state' in query:
                action = query['state'][0]
                if action == 'on':
                    state["active"] = True
                    state["timer_end_time"] = 0 # Manual ON cancels timer
                elif action == 'off':
                    state["active"] = False
                    state["timer_end_time"] = 0
                    
            if 'timer' in query:
                mins = int(query['timer'][0])
                if mins > 0:
                    state["active"] = True
                    state["timer_end_time"] = time.time() + (mins * 60)
                else:
                    state["timer_end_time"] = 0
            
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(b"OK")
            
        else:
            # Serve other static files if any
            return http.server.SimpleHTTPRequestHandler.do_GET(self)

print(f"Starting local test server at http://localhost:{PORT}")
print("Press Ctrl+C to stop")

with socketserver.TCPServer(("", PORT), MockHandler) as httpd:
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
