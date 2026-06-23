# GitHub Issues for Prakshep Engine

The following are 10 production-ready GitHub issues tailored for this repository. Maintainers should copy-paste these into GitHub to attract contributors.

---

## 🎨 UI/UX & Graphic Design Issues

### 1. [HELP WANTED] Design Procedural 2D/3D Rocket Meshes & SVGs for Indian Launchers (NGLV, LVM3, SSLV)

* **Labels:** `help wanted`, `design`, `enhancement`
* **Description:** We are building a SolidWorks-grade digital twin simulation engine for Indian space vehicles, but our current UI is using generic boxes and placeholders. We need talented technical artists and graphic designers to contribute production-ready visual assets.
* **Requirements:**
* Clean, vector-based SVG graphics of **NGLV (Soorya)**, **LVM3**, **PSLV-XL**, and **SSLV** structures.
* Broken-down stage components (Booster stages, strap-on solids, fairing halves) as separate layers/assets to support procedural staging animations.
* (Optional) Low-poly `.gltf`/`.glb` 3D meshes optimized for high-performance WebGPU rendering layers.



### 2. [ENHANCEMENT] Redesign Setup Panel Layout to Prevent Viewport Overflow

* **Labels:** `ui-ux`, `frontend`, `good first issue`
* **Description:** The current mission initialization configuration panel suffers from duplicate input parameters and a scrolling box container that overflows standard 16:9 screen layouts.
* **Tasks:**
* Consolidate input blocks into a strict 3-column configuration structure (Column 1: Vehicle Profiles, Column 2: Mission Parameters, Column 3: Launch Environment).
* Eliminate duplicate payload mass inputs and constraint sliders.
* Ensure the container strictly adheres to viewport boundaries without nesting vertical scrollbars inside scrollbars.



---

## 📈 Telemetry & Visualization Issues

### 3. [FEATURE] Implement Overlapping Nominal vs. Flight Trajectory Telemetry Charts

* **Labels:** `enhancement`, `data-viz`, `frontend`
* **Description:** Our current real-time telemetry graphs render live streams without context. For operational diagnostics, flight telemetry lines must overlay directly on top of pre-calculated static baseline reference lines.
* **Tasks:**
* Integrate an architecture to load a reference profile dataset (Nominal Ascent Path) depending on the selected launcher type and payload mass.
* Update line graphs to render the dotted nominal tracking path over the complete time horizon.
* Stream live incoming WebSocket coordinates dynamically over the nominal baseline line to immediately surface tracking errors.



### 4. [FEATURE] Replace Ground Trace Graph with an Interactive Zoomable Canvas Map

* **Labels:** `feature`, `canvas`, `frontend`
* **Description:** The current global tracking ground trace is rendered as a standard static plot. It needs to be a fully interactive tracking application.
* **Tasks:**
* Build an HTML5 Canvas or SVG map container mapped to an Equirectangular projection centered on the Indian Ocean launch corridor.
* Implement mouse-drag panning and scroll-wheel zoom scaling functionality.
* Draw the vehicle’s active downrange trail originating from SDSC-SHAR coordinates (13.72° N, 80.23° E) relative to the nominal path line.



---

## 🚀 Performance & Core Architecture Issues

### 5. [BUG] Remove Main-Thread Physics Math Interleaving from React Pipeline

* **Labels:** `bug`, `performance`, `critical`
* **Description:** The frontend simulation framerate is heavily choked, taking up to 22 seconds to record 1 km of simulated ascent altitude. This is because raw physics integration updates are bottlenecking the main JavaScript execution loop.
* **Tasks:**
* Audit and purge all structural iteration loops and state-heavy mathematical integration formulas from React component rendering cycles.
* Outsource tracking state management completely to background ingestion routines or delegate data loops to independent animation systems.



### 6. [ENHANCEMENT] Implement Throttling Buffer for High-Frequency WebSocket Streams

* **Labels:** `performance`, `websocket`, `backend-sync`
* **Description:** The C++ backend sends rapid telemetry state updates via WebSockets. If the client tries to re-render the DOM upon every incoming packet, the browser immediately bottlenecks.
* **Tasks:**
* Build an intermediate state buffer inside the WebSocket context.
* Throttle rendering state updates using a strict `requestAnimationFrame` loop to limit DOM updates to 60Hz, decoupling rendering speed from telemetry capture frequency.



---

## 🛠️ Data Integrity & Operational Robustness

### 7. [BUG] Add Explicit Error Catching to Mission Ignition API Dispatches

* **Labels:** `bug`, `robustness`, `good first issue`
* **Description:** When the initial setup API payload fails (e.g., due to an offline backend server or network timeout), the app crashes locally or hangs indefinitely on Vercel deployment without surfacing warnings to the user.
* **Tasks:**
* Wrap the `fetch`/`axios` instantiation inside `SetupPanel.tsx` in a robust `try/catch` block.
* Intercept bad server responses and expose specific runtime warnings to user diagnostics instead of allowing silent unhandled promise rejections.



### 8. [ENHANCEMENT] Synchronize Frontend JSON Schema Variables with Python Pydantic Models

* **Labels:** `bug`, `fastapi`, `pydantic`
* **Description:** Altering launcher options throws an HTTP 422 Unprocessable Entity error because frontend input properties mismatch the variable models inside our FastAPI validation schemas (`schemas.py`).
* **Tasks:**
* Standardize all runtime configurations (payload mass, orbital targets, weather options) to match snake_case/camelCase criteria across client payloads and server specifications.
* Expand the Python launcher Enum strings to natively accept all custom test vehicle variants.



### 9. [FEATURE] Build Automatic Orbit Type Logic Based on Altitude Inputs

* **Labels:** `enhancement`, `good first issue`
* **Description:** The orbit profile checklist requires manual input coordination. Entering values above LEO ranges (e.g., 35,000 km) should programmatically transition the state tracking targets.
* **Tasks:**
* Add a listener function inside the mission setup handler to watch apogee/perigee values.
* Auto-toggle target profiles (e.g., clear LEO and check GTO/GEO) if input values exceed 2,000 km.
* Add predefined state overrides for standard configurations like SSO (500 km altitude, 97.4° inclination).



### 10. [ENHANCEMENT] Expose FastAPI Validation Breakdown Logs directly to Server Console

* **Labels:** `dx`, `backend`, `fastapi`
* **Description:** When payloads violate Pydantic rules, FastAPI rejects requests silently with a generic 422 error, slowing down local developer workflow.
* **Tasks:**
* Register a global `RequestValidationError` handling override inside the backend application server.
* Format and print incoming validation tracebacks (`exc.errors()`) directly to the active terminal window to immediately reveal structural data typos.
