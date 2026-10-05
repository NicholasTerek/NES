import * as THREE from "three/webgpu";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";

const TABLE_TOP_Y = 1.31;
const MIN_PIXEL_RATIO = 1;
const MAX_PIXEL_RATIO = 1.6;

function bakedMaterial(color, map = null) {
  return new THREE.MeshBasicMaterial({ color, map, toneMapped: true });
}

function litMaterial(color, roughness = 0.72, metalness = 0) {
  return new THREE.MeshStandardMaterial({ color, roughness, metalness });
}

function addMesh(parent, geometry, material, position) {
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(...position);
  mesh.castShadow = false;
  mesh.receiveShadow = false;
  parent.add(mesh);
  return mesh;
}

function createFloorTexture() {
  const canvas = document.createElement("canvas");
  canvas.width = 512;
  canvas.height = 512;
  const ctx = canvas.getContext("2d");
  const plankHeight = 32;
  const colors = ["#633920", "#704227", "#5b321e", "#684028"];

  for (let y = 0; y < canvas.height; y += plankHeight) {
    ctx.fillStyle = colors[(y / plankHeight) % colors.length];
    ctx.fillRect(0, y, canvas.width, plankHeight);
    ctx.fillStyle = "rgba(20,10,6,.18)";
    ctx.fillRect(0, y, canvas.width, 2);

    const offset = ((y / plankHeight) % 2) * 128;
    for (let x = -offset; x < canvas.width; x += 256) {
      ctx.fillRect(x, y, 2, plankHeight);
    }
  }

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.wrapS = THREE.RepeatWrapping;
  texture.wrapT = THREE.RepeatWrapping;
  texture.repeat.set(1.5, 1.15);
  texture.anisotropy = 4;
  return texture;
}

function createContactShadowTexture() {
  const canvas = document.createElement("canvas");
  canvas.width = 128;
  canvas.height = 128;
  const ctx = canvas.getContext("2d");
  const gradient = ctx.createRadialGradient(64, 64, 8, 64, 64, 62);
  gradient.addColorStop(0, "rgba(0,0,0,.42)");
  gradient.addColorStop(0.45, "rgba(0,0,0,.22)");
  gradient.addColorStop(1, "rgba(0,0,0,0)");
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, 128, 128);

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return texture;
}

function addContactShadow(scene, texture, x, z, width, depth, opacity = 1) {
  const shadow = new THREE.Mesh(
    new THREE.PlaneGeometry(width, depth),
    new THREE.MeshBasicMaterial({
      map: texture,
      transparent: true,
      opacity,
      depthWrite: false,
      toneMapped: false,
    })
  );
  shadow.rotation.x = -Math.PI / 2;
  shadow.position.set(x, TABLE_TOP_Y + 0.008, z);
  shadow.renderOrder = 2;
  scene.add(shadow);
}

function buildRoom() {
  const room = new THREE.Group();
  const floorTexture = createFloorTexture();

  addMesh(
    room,
    new THREE.BoxGeometry(12, 0.07, 10),
    bakedMaterial(0xffffff, floorTexture),
    [0, -0.035, 0]
  );

  addMesh(
    room,
    new THREE.BoxGeometry(12, 6.1, 0.16),
    bakedMaterial(0x6a2923),
    [0, 3, -3.85]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.16, 6.1, 9.3),
    bakedMaterial(0x501d20),
    [-6, 3, 0]
  );

  addMesh(
    room,
    new THREE.BoxGeometry(12, 0.25, 0.12),
    bakedMaterial(0xc5ae79),
    [0, 1.03, -3.74]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.12, 0.25, 9.1),
    bakedMaterial(0xb89d6e),
    [-5.90, 1.03, 0]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(12, 0.14, 0.12),
    bakedMaterial(0x8d7449),
    [0, 5.82, -3.74]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.12, 0.14, 9.1),
    bakedMaterial(0x806843),
    [-5.90, 5.82, 0]
  );

  addMesh(
    room,
    new THREE.CylinderGeometry(4.35, 4.35, 0.04, 64),
    bakedMaterial(0x101f1b),
    [-1.65, 0.025, -0.30]
  );
  addMesh(
    room,
    new THREE.CylinderGeometry(3.65, 3.65, 0.24, 64),
    litMaterial(0x17352e, 0.84),
    [-2.35, 1.18, -1.00]
  );
  addMesh(
    room,
    new THREE.CylinderGeometry(0.70, 1.18, 1.05, 32),
    bakedMaterial(0x2d2722),
    [-2.35, 0.62, -1.00]
  );

  return room;
}

function fitModel(root, targetSize) {
  const box = new THREE.Box3().setFromObject(root);
  const size = box.getSize(new THREE.Vector3());
  const maxAxis = Math.max(size.x, size.y, size.z);

  if (maxAxis > 0) {
    root.scale.multiplyScalar(targetSize / maxAxis);
  }
}

function placeByBottomCenter(root, x, y, z) {
  const box = new THREE.Box3().setFromObject(root);
  const center = box.getCenter(new THREE.Vector3());

  root.position.x += x - center.x;
  root.position.y += y - box.min.y;
  root.position.z += z - center.z;
}

function optimizeModel(root) {
  root.traverse((object) => {
    if (!object.isMesh) return;
    object.castShadow = false;
    object.receiveShadow = false;
    object.frustumCulled = true;

    const materials = Array.isArray(object.material) ? object.material : [object.material];
    for (const material of materials) {
      if (!material) continue;
      material.needsUpdate = true;
    }
  });
}

function addCrtScreenOverlay(scene, tv, screenMaterial) {
  const box = new THREE.Box3().setFromObject(tv);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());
  const width = size.x * 0.80;
  const height = width * 0.75;

  const screen = new THREE.Mesh(new THREE.PlaneGeometry(width, height), screenMaterial);
  screen.position.set(
    center.x - size.x * 0.005,
    center.y + size.y * 0.07,
    box.max.z - 0.02
  );
  screen.rotation.y = tv.rotation.y;
  screen.renderOrder = 10;
  scene.add(screen);
}

async function loadAssets(scene, screenMaterial) {
  const loader = new GLTFLoader();
  const shadowTexture = createContactShadowTexture();

  const [nesGltf, tvGltf, cartridgeGltf] = await Promise.all([
    loader.loadAsync("./assets/nes_console_and_controller.glb"),
    loader.loadAsync("./assets/sony_pvm-14l2_crt_tv.glb"),
    loader.loadAsync("./assets/nes_cartridge__super_mario_bros.glb"),
  ]);

  const nes = nesGltf.scene;
  fitModel(nes, 2.15);
  nes.rotation.set(0, 0, 0);
  placeByBottomCenter(nes, -0.25, TABLE_TOP_Y, -0.60);
  optimizeModel(nes);
  scene.add(nes);
  addContactShadow(scene, shadowTexture, -0.25, -0.55, 2.45, 1.35, 0.85);

  const tv = tvGltf.scene;
  fitModel(tv, 3.05);
  tv.rotation.set(0, 0, 0);
  placeByBottomCenter(tv, -2.35, TABLE_TOP_Y, -2.20);
  optimizeModel(tv);
  scene.add(tv);
  addContactShadow(scene, shadowTexture, -2.35, -2.00, 2.9, 1.35, 0.95);
  addCrtScreenOverlay(scene, tv, screenMaterial);

  const cartridge = cartridgeGltf.scene;
  fitModel(cartridge, 0.98);
  cartridge.rotation.set(-0.04, 0, 0);
  placeByBottomCenter(cartridge, -4.60, TABLE_TOP_Y, -0.60);
  optimizeModel(cartridge);
  scene.add(cartridge);
  addContactShadow(scene, shadowTexture, -4.60, -0.56, 1.0, 0.58, 0.7);

  return { cartridge };
}

function initialPixelRatio() {
  const deviceMemory = navigator.deviceMemory ?? 8;
  const cores = navigator.hardwareConcurrency ?? 8;
  const budget = deviceMemory <= 4 || cores <= 4 ? 1.2 : MAX_PIXEL_RATIO;
  return Math.min(devicePixelRatio || 1, budget);
}

export async function createScene(stage, nesCanvas, onCartridgeClick) {
  const renderer = new THREE.WebGPURenderer({
    antialias: true,
    powerPreference: "high-performance",
  });
  await renderer.init();

  let pixelRatio = initialPixelRatio();
  renderer.setPixelRatio(pixelRatio);
  renderer.setSize(stage.clientWidth, stage.clientHeight);
  renderer.shadowMap.enabled = false;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.18;
  stage.appendChild(renderer.domElement);

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x080606);
  scene.fog = new THREE.Fog(0x080606, 10.5, 22);
  scene.add(buildRoom());

  const camera = new THREE.PerspectiveCamera(
    46,
    stage.clientWidth / stage.clientHeight,
    0.05,
    100
  );
  camera.position.set(-2.35, 2.35, 4.55);

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.enablePan = false;
  controls.target.set(-2.35, 1.95, -1.05);
  controls.minDistance = 5.6;
  controls.maxDistance = 7.4;
  controls.minPolarAngle = 1.38;
  controls.maxPolarAngle = 1.50;
  controls.minAzimuthAngle = -0.18;
  controls.maxAzimuthAngle = 0.18;

  scene.add(new THREE.HemisphereLight(0xffdfbd, 0x2e2020, 1.55));

  const keyLight = new THREE.PointLight(0xffc07a, 58, 18, 2);
  keyLight.position.set(0.0, 3.4, 5.8);
  scene.add(keyLight);

  const fillLight = new THREE.PointLight(0xffb36b, 12, 10, 2);
  fillLight.position.set(-2.8, 2.8, 4.8);
  scene.add(fillLight);

  const nesTexture = new THREE.CanvasTexture(nesCanvas);
  nesTexture.colorSpace = THREE.SRGBColorSpace;
  nesTexture.magFilter = THREE.NearestFilter;
  nesTexture.minFilter = THREE.NearestFilter;
  nesTexture.generateMipmaps = false;

  const screenMaterial = new THREE.MeshBasicMaterial({
    map: nesTexture,
    toneMapped: false,
  });

  let assetsLoaded = false;
  let cartridge = null;

  try {
    ({ cartridge } = await loadAssets(scene, screenMaterial));
    assetsLoaded = true;
  } catch (error) {
    console.error("Could not load GLB assets:", error);
  }

  const raycaster = new THREE.Raycaster();
  const pointer = new THREE.Vector2();
  let needsRender = true;
  let sampleFrames = 0;
  let sampleTime = 0;
  let lastRenderTime = performance.now();

  function cartridgeHit(event) {
    if (!cartridge) return false;

    const rect = renderer.domElement.getBoundingClientRect();
    pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    raycaster.setFromCamera(pointer, camera);

    return raycaster.intersectObject(cartridge, true).length > 0;
  }

  renderer.domElement.addEventListener("pointermove", (event) => {
    renderer.domElement.style.cursor = cartridgeHit(event) ? "pointer" : "grab";
  });

  renderer.domElement.addEventListener("click", (event) => {
    if (!cartridgeHit(event)) return;
    event.stopPropagation();
    onCartridgeClick?.();
  });

  addEventListener("resize", () => {
    const width = stage.clientWidth;
    const height = stage.clientHeight;
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
    needsRender = true;
  });

  function adaptPixelRatio(now) {
    const elapsed = now - lastRenderTime;
    lastRenderTime = now;
    if (elapsed <= 0 || elapsed > 100) return;

    sampleTime += elapsed;
    sampleFrames += 1;
    if (sampleFrames < 120) return;

    const averageMs = sampleTime / sampleFrames;
    const cap = Math.min(devicePixelRatio || 1, MAX_PIXEL_RATIO);
    let next = pixelRatio;

    if (averageMs > 19 && pixelRatio > MIN_PIXEL_RATIO) {
      next = Math.max(MIN_PIXEL_RATIO, pixelRatio - 0.15);
    } else if (averageMs < 15 && pixelRatio < cap) {
      next = Math.min(cap, pixelRatio + 0.1);
    }

    if (Math.abs(next - pixelRatio) >= 0.05) {
      pixelRatio = next;
      renderer.setPixelRatio(pixelRatio);
      renderer.setSize(stage.clientWidth, stage.clientHeight, false);
      needsRender = true;
    }

    sampleFrames = 0;
    sampleTime = 0;
  }

  return {
    render(frameUpdated = false) {
      const now = performance.now();
      adaptPixelRatio(now);
      const controlsChanged = controls.update();

      if (!frameUpdated && !controlsChanged && !needsRender) return;
      if (frameUpdated) nesTexture.needsUpdate = true;

      renderer.render(scene, camera);
      needsRender = false;
    },
    renderer,
    assetsLoaded,
  };
}
