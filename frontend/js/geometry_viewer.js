/**
 * Unified Engineering Workbench - 3D Geometry & Temperature Viewer
 * Three.js WebGL CAD rendering, ANSYS CSV point cloud & interactive node inspection
 */

class GeometryViewer {
    constructor() {
        this.container = document.getElementById('viewport-3d');
        if (!this.container) return;

        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.mesh = null;
        this.wireframeMesh = null;
        this.pointCloud = null;
        this.dotTexture = null;

        this.raycaster = new THREE.Raycaster();
        this.mouse = new THREE.Vector2();
        this.activeNodes = [];
        this.hoveredNode = null;

        this.currentColormap = 'turbo';
        this.wireframeVisible = true;
        this.meshOpacity = 0.85;
        this.pointSize = 10.0; // Screen-space pixel diameter

        this.tMin = 0.0;
        this.tMax = 100.0;
        this.hasThermalData = false;

        this.initThree();
        this.setupEventListeners();
        this.setupToolbar();
    }

    initThree() {
        const width = this.container.clientWidth || 800;
        const height = this.container.clientHeight || 550;

        // Scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x06090e);

        // Camera: FOV 45, Near 0.001 m (1 mm), Far 50.0 m
        this.camera = new THREE.PerspectiveCamera(45, width / height, 0.001, 50.0);
        // Standard CAD coordinate system: Z is UP
        this.camera.up.set(0, 0, 1);
        this.camera.position.set(0.20, -0.22, 0.16);

        // Renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        this.renderer.setSize(width, height);
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
        this.renderer.outputEncoding = THREE.sRGBEncoding;
        this.container.appendChild(this.renderer.domElement);

        // OrbitControls
        this.controls = new THREE.OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.08;
        this.controls.screenSpacePanning = true;
        this.controls.target.set(0.05, 0.025, 0.04); // Default center of benchmark geometry

        // Balanced CAD Studio Lighting (Z-Up oriented)
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.65);
        this.scene.add(ambientLight);

        // Key light (top-front-right)
        const keyLight = new THREE.DirectionalLight(0xffffff, 0.85);
        keyLight.position.set(1.0, -1.0, 2.0).normalize();
        this.scene.add(keyLight);

        // Fill light (top-back-left)
        const fillLight = new THREE.DirectionalLight(0x88aacc, 0.50);
        fillLight.position.set(-1.0, 1.0, 1.5).normalize();
        this.scene.add(fillLight);

        // Rim / Under light
        const rimLight = new THREE.DirectionalLight(0x446688, 0.30);
        rimLight.position.set(0.0, 0.0, -1.0).normalize();
        this.scene.add(rimLight);

        // Engineering Helpers (oriented on ground XY plane, Z is UP)
        const grid = new THREE.GridHelper(0.3, 30, 0x388bfd, 0x1f2a3f);
        grid.rotation.x = Math.PI / 2; // Lie flat in XY plane
        grid.position.z = -0.0005;
        this.scene.add(grid);

        // Axes helper (Red = X, Green = Y, Blue = Z [UP])
        const axes = new THREE.AxesHelper(0.05);
        this.scene.add(axes);

        // Generate circular disc texture for nodes
        this.dotTexture = this.createCircularSprite();

        // Raycaster sensitivity for screen-space points
        this.raycaster.params.Points.threshold = 0.0035;

        // Animation loop
        const animate = () => {
            requestAnimationFrame(animate);
            this.controls.update();
            this.renderer.render(this.scene, this.camera);
        };
        animate();

        // Resize observer
        window.addEventListener('resize', () => this.onWindowResize());
        const resizeObserver = new ResizeObserver(() => this.onWindowResize());
        resizeObserver.observe(this.container);
    }

    createCircularSprite() {
        const canvas = document.createElement('canvas');
        canvas.width = 64;
        canvas.height = 64;
        const ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, 64, 64);

        ctx.beginPath();
        ctx.arc(32, 32, 26, 0, Math.PI * 2);
        ctx.fillStyle = '#ffffff';
        ctx.fill();

        ctx.lineWidth = 4;
        ctx.strokeStyle = 'rgba(0, 0, 0, 0.75)';
        ctx.stroke();

        const texture = new THREE.CanvasTexture(canvas);
        texture.minFilter = THREE.LinearFilter;
        texture.magFilter = THREE.LinearFilter;
        return texture;
    }

    onWindowResize() {
        if (!this.container || !this.renderer || !this.camera) return;
        const width = this.container.clientWidth;
        const height = this.container.clientHeight;
        if (width === 0 || height === 0) return;

        this.camera.aspect = width / height;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(width, height);
    }

    evaluateColormap(u, colormap = this.currentColormap) {
        u = Math.max(0.0, Math.min(1.0, u));

        if (colormap === 'jet') {
            // Thermal Jet: Blue -> Cyan -> Green -> Yellow -> Red
            const r = Math.max(0.0, Math.min(1.0, 1.5 - Math.abs(u * 4 - 3)));
            const g = Math.max(0.0, Math.min(1.0, 1.5 - Math.abs(u * 4 - 2)));
            const b = Math.max(0.0, Math.min(1.0, 1.5 - Math.abs(u * 4 - 1)));
            return new THREE.Color(r, g, b);
        } else if (colormap === 'viridis') {
            // Scientific Viridis
            const r = -0.1 + 1.2 * u - 0.2 * u * u;
            const g = 0.1 + 0.9 * Math.sin(u * Math.PI * 0.9);
            const b = 0.5 - 0.8 * u + 0.9 * Math.sqrt(Math.max(0, 1 - u));
            return new THREE.Color(
                Math.max(0, Math.min(1, r)),
                Math.max(0, Math.min(1, g)),
                Math.max(0, Math.min(1, b))
            );
        } else {
            // Google Turbo Colormap (Default for high-contrast thermal analysis)
            const r = 0.1357 + u * (4.5974 - u * (42.345 + u * (-130.58 + u * (54.16 - u * 45.8))));
            const g = 0.0914 + u * (2.1856 + u * (4.8052 - u * (14.0195 + u * (4.21 - u * 3.1))));
            const b = 0.1067 + u * (12.573 - u * (86.19 + u * (221.4 - u * (268.0 - u * 118.0))));
            return new THREE.Color(
                Math.max(0, Math.min(1, r)),
                Math.max(0, Math.min(1, g)),
                Math.max(0, Math.min(1, b))
            );
        }
    }

    renderStlGeometry(geomData) {
        if (!geomData || !geomData.triangles || geomData.triangles.length === 0) {
            window.showToast?.('Invalid or empty STL geometry dataset received.', 'error');
            return;
        }

        // Clean up previous mesh objects safely to avoid duplicate GPU handles
        if (this.mesh) {
            this.scene.remove(this.mesh);
            this.mesh.geometry.dispose();
            if (this.mesh.material) this.mesh.material.dispose();
            this.mesh = null;
        }
        if (this.wireframeMesh) {
            this.scene.remove(this.wireframeMesh);
            this.wireframeMesh.geometry.dispose();
            if (this.wireframeMesh.material) this.wireframeMesh.material.dispose();
            this.wireframeMesh = null;
        }

        const triCount = geomData.triangles.length;
        const positions = new Float32Array(triCount * 9);
        const normals = new Float32Array(triCount * 9);

        for (let i = 0; i < triCount; ++i) {
            const tri = geomData.triangles[i];
            const base = i * 9;

            // Direct native right-handed coordinates: X, Y, Z (no axis swapping!)
            positions[base + 0] = tri.v1.x;
            positions[base + 1] = tri.v1.y;
            positions[base + 2] = tri.v1.z;

            positions[base + 3] = tri.v2.x;
            positions[base + 4] = tri.v2.y;
            positions[base + 5] = tri.v2.z;

            positions[base + 6] = tri.v3.x;
            positions[base + 7] = tri.v3.y;
            positions[base + 8] = tri.v3.z;

            for (let v = 0; v < 3; ++v) {
                normals[base + v * 3 + 0] = tri.normal.x;
                normals[base + v * 3 + 1] = tri.normal.y;
                normals[base + v * 3 + 2] = tri.normal.z;
            }
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
        geometry.computeVertexNormals();

        // Neutral Titanium/Steel Engineering CAD Material
        const material = new THREE.MeshStandardMaterial({
            color: 0x6e8294,
            roughness: 0.38,
            metalness: 0.30,
            transparent: true,
            opacity: this.meshOpacity,
            side: THREE.DoubleSide,
            depthWrite: true,
            polygonOffset: true,
            polygonOffsetFactor: 1,
            polygonOffsetUnits: 1
        });

        this.mesh = new THREE.Mesh(geometry, material);
        this.scene.add(this.mesh);

        // High-contrast wireframe lines
        const wireframeGeom = new THREE.WireframeGeometry(geometry);
        const wireframeMat = new THREE.LineBasicMaterial({
            color: 0x38bdf8,
            transparent: true,
            opacity: 0.45
        });
        this.wireframeMesh = new THREE.LineSegments(wireframeGeom, wireframeMat);
        this.wireframeMesh.visible = this.wireframeVisible;
        this.scene.add(this.wireframeMesh);

        // Update HUD metrics
        document.getElementById('hud-solid-name').innerText = geomData.solid_name || 'CAD Solid';
        document.getElementById('hud-tri-count').innerText = `${geomData.triangle_count.toLocaleString()} facets`;
        document.getElementById('hud-vert-count').innerText = `${geomData.vertex_count.toLocaleString()} coords`;
        document.getElementById('hud-surface-area').innerText = 
            `${(geomData.surface_area * 1e6).toFixed(1)} mm² (${geomData.surface_area.toFixed(5)} m²)`;

        const bb = geomData.bounding_box.dimensions;
        document.getElementById('hud-dimensions').innerText = 
            `${(bb.x * 1000).toFixed(1)} × ${(bb.y * 1000).toFixed(1)} × ${(bb.z * 1000).toFixed(1)} mm`;

        // Frame the geometry nicely
        this.fitToView();
    }

    renderPointCloud(cloudData) {
        if (!cloudData || !cloudData.nodes || cloudData.nodes.length === 0) {
            window.showToast?.('Invalid or empty thermal point cloud dataset received.', 'error');
            return;
        }

        // Clean up previous point cloud
        if (this.pointCloud) {
            this.scene.remove(this.pointCloud);
            this.pointCloud.geometry.dispose();
            if (this.pointCloud.material) this.pointCloud.material.dispose();
            this.pointCloud = null;
        }

        this.activeNodes = cloudData.nodes;
        this.tMin = cloudData.min_temperature_C;
        this.tMax = cloudData.max_temperature_C;
        this.hasThermalData = true;

        const count = cloudData.nodes.length;
        const positions = new Float32Array(count * 3);
        const colors = new Float32Array(count * 3);

        const tempRange = (this.tMax - this.tMin) || 1.0;

        for (let i = 0; i < count; ++i) {
            const node = cloudData.nodes[i];
            // Native right-handed position (X, Y, Z)
            positions[i * 3 + 0] = node.x;
            positions[i * 3 + 1] = node.y;
            positions[i * 3 + 2] = node.z;

            const u = (node.temperature - this.tMin) / tempRange;
            const col = this.evaluateColormap(u, this.currentColormap);

            colors[i * 3 + 0] = col.r;
            colors[i * 3 + 1] = col.g;
            colors[i * 3 + 2] = col.b;
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        // Screen-space points (sizeAttenuation = false) prevents gigantic oversized rainbow discs
        const material = new THREE.PointsMaterial({
            size: this.pointSize,
            vertexColors: true,
            map: this.dotTexture,
            transparent: true,
            alphaTest: 0.05,
            sizeAttenuation: false
        });

        this.pointCloud = new THREE.Points(geometry, material);
        this.scene.add(this.pointCloud);

        // Update Legend & HUD
        this.updateLegend();
        document.getElementById('hud-node-count').innerText = `${count.toLocaleString()} nodes`;
        document.getElementById('hud-temp-range').innerText = `${this.tMin.toFixed(2)} °C to ${this.tMax.toFixed(2)} °C`;
        document.getElementById('hud-temp-mean').innerText = `${cloudData.mean_temperature_C.toFixed(2)} °C`;

        // If only point cloud was loaded without STL, fit view to point cloud
        if (!this.mesh) {
            this.fitToView();
        }
    }

    updateLegend() {
        const legendPanel = document.querySelector('.legend-panel');
        if (!legendPanel) return;

        if (!this.hasThermalData) {
            legendPanel.style.display = 'none';
            return;
        }
        legendPanel.style.display = 'flex';

        const bar = document.getElementById('legend-gradient-bar');
        const tMaxEl = document.getElementById('legend-tmax');
        const tMidEl = document.getElementById('legend-tmid');
        const tMinEl = document.getElementById('legend-tmin');

        if (tMaxEl) tMaxEl.innerText = `${this.tMax.toFixed(1)} °C`;
        if (tMidEl) tMidEl.innerText = `${(0.5 * (this.tMin + this.tMax)).toFixed(1)} °C`;
        if (tMinEl) tMinEl.innerText = `${this.tMin.toFixed(1)} °C`;

        if (!bar) return;

        // Generate linear gradient CSS
        const steps = 10;
        let gradStops = [];
        for (let i = steps; i >= 0; --i) {
            const u = i / steps;
            const c = this.evaluateColormap(u, this.currentColormap);
            const r = Math.round(c.r * 255);
            const g = Math.round(c.g * 255);
            const b = Math.round(c.b * 255);
            gradStops.push(`rgb(${r}, ${g}, ${b})`);
        }
        bar.style.background = `linear-gradient(to bottom, ${gradStops.join(', ')})`;
    }

    fitToView() {
        const box = new THREE.Box3();
        if (this.mesh) {
            box.expandByObject(this.mesh);
        }
        if (this.pointCloud) {
            box.expandByObject(this.pointCloud);
        }

        if (box.isEmpty()) return;

        const center = new THREE.Vector3();
        box.getCenter(center);

        const sphere = box.getBoundingSphere(new THREE.Sphere());
        const radius = sphere.radius || 0.05;

        this.controls.target.copy(center);

        const fov = this.camera.fov * (Math.PI / 180);
        const aspect = this.camera.aspect || 1.0;
        const fovH = 2 * Math.atan(Math.tan(fov / 2) * aspect);
        const effectiveFov = Math.min(fov, fovH);

        const distance = (radius / Math.sin(effectiveFov / 2)) * 1.35;

        // Position camera in engineering isometric viewpoint with Z-Up
        const dir = new THREE.Vector3(1.3, -1.5, 1.1).normalize();
        this.camera.position.copy(center).add(dir.multiplyScalar(distance));
        this.camera.lookAt(center);
        this.controls.update();
    }

    resetView() {
        this.fitToView();
    }

    toggleWireframe() {
        this.wireframeVisible = !this.wireframeVisible;
        if (this.wireframeMesh) {
            this.wireframeMesh.visible = this.wireframeVisible;
        }
        const btn = document.getElementById('btn-toggle-wireframe');
        if (btn) btn.classList.toggle('active', this.wireframeVisible);
    }

    setMeshOpacity(val) {
        this.meshOpacity = Math.max(0.0, Math.min(1.0, parseFloat(val)));
        if (this.mesh && this.mesh.material) {
            this.mesh.material.opacity = this.meshOpacity;
            this.mesh.material.transparent = this.meshOpacity < 1.0;
        }
    }

    setPointSize(val) {
        this.pointSize = Math.max(2.0, Math.min(30.0, parseFloat(val)));
        if (this.pointCloud && this.pointCloud.material) {
            this.pointCloud.material.size = this.pointSize;
        }
    }

    setColormap(name) {
        this.currentColormap = name;
        if (this.pointCloud && this.activeNodes.length > 0) {
            const count = this.activeNodes.length;
            const colors = this.pointCloud.geometry.attributes.color.array;
            const tempRange = (this.tMax - this.tMin) || 1.0;

            for (let i = 0; i < count; ++i) {
                const u = (this.activeNodes[i].temperature - this.tMin) / tempRange;
                const col = this.evaluateColormap(u, this.currentColormap);
                colors[i * 3 + 0] = col.r;
                colors[i * 3 + 1] = col.g;
                colors[i * 3 + 2] = col.b;
            }
            this.pointCloud.geometry.attributes.color.needsUpdate = true;
            this.updateLegend();
        }
    }

    setupEventListeners() {
        const tooltip = document.getElementById('node-tooltip');

        this.container.addEventListener('mousemove', (e) => {
            const rect = this.renderer.domElement.getBoundingClientRect();
            this.mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
            this.mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

            if (!this.pointCloud || this.activeNodes.length === 0) {
                if (tooltip) tooltip.style.display = 'none';
                return;
            }

            this.raycaster.setFromCamera(this.mouse, this.camera);
            const intersects = this.raycaster.intersectObject(this.pointCloud);

            if (intersects.length > 0) {
                const index = intersects[0].index;
                const node = this.activeNodes[index];
                if (node) {
                    this.hoveredNode = node;
                    if (tooltip) {
                        tooltip.style.display = 'block';
                        tooltip.style.left = `${e.clientX - rect.left + 15}px`;
                        tooltip.style.top = `${e.clientY - rect.top + 15}px`;
                        tooltip.innerHTML = `
                            <strong>Node #${node.node_id}</strong><br>
                            Temp: <span style="color:var(--accent-cyan); font-weight:bold;">${node.temperature.toFixed(2)} °C</span><br>
                            X: ${node.x.toFixed(4)} m<br>
                            Y: ${node.y.toFixed(4)} m<br>
                            Z: ${node.z.toFixed(4)} m
                        `;
                    }

                    // Update sidebar inspection card
                    document.getElementById('inspect-node-id').innerText = `#${node.node_id}`;
                    document.getElementById('inspect-node-temp').innerText = `${node.temperature.toFixed(3)} °C`;
                    document.getElementById('inspect-node-pos').innerText = 
                        `(${node.x.toFixed(4)}, ${node.y.toFixed(4)}, ${node.z.toFixed(4)}) m`;
                }
            } else {
                if (tooltip) tooltip.style.display = 'none';
            }
        });
    }

    setupToolbar() {
        document.getElementById('btn-fit-view')?.addEventListener('click', () => this.fitToView());
        document.getElementById('btn-reset-view')?.addEventListener('click', () => this.resetView());
        document.getElementById('btn-toggle-wireframe')?.addEventListener('click', () => this.toggleWireframe());

        document.getElementById('select-colormap')?.addEventListener('change', (e) => {
            this.setColormap(e.target.value);
        });

        document.getElementById('slider-mesh-opacity')?.addEventListener('input', (e) => {
            document.getElementById('val-mesh-opacity').innerText = `${Math.round(e.target.value * 100)}%`;
            this.setMeshOpacity(e.target.value);
        });

        document.getElementById('slider-point-size')?.addEventListener('input', (e) => {
            document.getElementById('val-point-size').innerText = `${e.target.value} px`;
            this.setPointSize(e.target.value);
        });

        // Sample Loaders
        document.getElementById('btn-load-sample-stl')?.addEventListener('click', async () => {
            window.showToast?.('Fetching C++ parsed benchmark STL...', 'info');
            const res = await window.api.request('/api/geometry/sample-stl');
            if (res.success && res.data && res.data.success) {
                this.renderStlGeometry(res.data.geometry);
                window.showToast?.('Benchmark STL parsed and rendered successfully.', 'success');
            } else {
                window.showToast?.(res.data?.error_message || 'Failed to load sample STL', 'error');
            }
        });

        document.getElementById('btn-load-sample-csv')?.addEventListener('click', async () => {
            window.showToast?.('Fetching C++ parsed ANSYS thermal field...', 'info');
            const res = await window.api.request('/api/geometry/sample-csv');
            if (res.success && res.data && res.data.success) {
                this.renderPointCloud(res.data.cloud);
                window.showToast?.('ANSYS CSV cloud loaded and colormapped.', 'success');
            } else {
                window.showToast?.(res.data?.error_message || 'Failed to load sample CSV', 'error');
            }
        });

        // Custom File Pickers
        const fileStlInput = document.getElementById('file-input-stl');
        document.getElementById('btn-upload-stl')?.addEventListener('click', () => fileStlInput?.click());
        fileStlInput?.addEventListener('change', async (e) => {
            const file = e.target.files[0];
            if (!file) return;

            window.showToast?.(`Uploading ${file.name} to C++ STL parser...`, 'info');
            const text = await file.text();
            const res = await window.api.request('/api/geometry/parse-stl', {
                method: 'POST',
                body: text
            });

            if (res.success && res.data && res.data.success) {
                this.renderStlGeometry(res.data.geometry);
                window.showToast?.(`STL loaded: ${res.data.geometry.triangle_count} facets`, 'success');
            } else {
                const errMsg = res.data?.error_message || res.error || 'STL Parse Error';
                window.showToast?.(errMsg, 'error');
                alert(`C++ STL Parser Validation Error:\n\n${errMsg}`);
            }
            fileStlInput.value = '';
        });

        const fileCsvInput = document.getElementById('file-input-csv');
        document.getElementById('btn-upload-csv')?.addEventListener('click', () => fileCsvInput?.click());
        fileCsvInput?.addEventListener('change', async (e) => {
            const file = e.target.files[0];
            if (!file) return;

            window.showToast?.(`Uploading ${file.name} to C++ ANSYS validator...`, 'info');
            const text = await file.text();
            const res = await window.api.request('/api/geometry/parse-csv', {
                method: 'POST',
                body: text
            });

            if (res.success && res.data && res.data.success) {
                this.renderPointCloud(res.data.cloud);
                window.showToast?.(`ANSYS CSV loaded: ${res.data.cloud.node_count} thermal nodes`, 'success');
            } else {
                const errMsg = res.data?.error_message || res.error || 'CSV Parse Error';
                window.showToast?.(errMsg, 'error');
                alert(`C++ ANSYS CSV Validator Error:\n\n${errMsg}`);
            }
            fileCsvInput.value = '';
        });
    }
}

// Global viewer instance
window.geometryViewer = null;
document.addEventListener('DOMContentLoaded', () => {
    window.geometryViewer = new GeometryViewer();

    const geomTab = document.querySelector('.nav-item[data-module="geometry"]');
    let initialLoadDone = false;

    const triggerInitialLoad = () => {
        setTimeout(() => {
            if (window.geometryViewer) {
                window.geometryViewer.onWindowResize();
                if (!initialLoadDone) {
                    initialLoadDone = true;
                    document.getElementById('btn-load-sample-stl')?.click();
                    setTimeout(() => {
                        document.getElementById('btn-load-sample-csv')?.click();
                    }, 350);
                }
            }
        }, 150);
    };

    if (geomTab) {
        geomTab.addEventListener('click', triggerInitialLoad);
    }

    if (window.location.hash === '#geometry') {
        triggerInitialLoad();
    }
});
