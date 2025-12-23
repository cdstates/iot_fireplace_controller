# Local Web Interface Test

This folder contains a local test environment for the Smart Fireplace web interface. It allows you to modify and test the HTML/JS/CSS without needing to flash the ESP32.

## Files

*   `index.html`: A copy of the web interface code. **Note:** If you make changes here, you must manually copy them back to `include/web_page.h` in the main project.
*   `server.py`: A simple Python script that acts as a mock ESP32. It serves the HTML file and responds to `/status` and `/control` API calls just like the real device.

## How to Run

1.  Ensure you have Python installed.
2.  Open a terminal in this folder (`test/local_server`).
3.  Run the server:
    ```bash
    python server.py
    ```
4.  Open your web browser and go to:
    [http://localhost:8000](http://localhost:8000)

## Testing

*   Click "Turn ON" / "Turn OFF" to see the status box change.
*   Set a timer to see the countdown work.
*   The "Thermocouple" and "Wall Switch" values are hardcoded mocks in `server.py`, but the UI logic will respond to them.
