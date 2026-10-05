import * as THREE from "three/webgpu";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";

const TABLE_TOP_Y = 1.31;

function standardMaterial(color, roughness = 0.72, metalness = 0) {
  return new THREE.MeshStandardMaterial({ color, roughness, metalness });
}

function addMesh(parent, geometry, meshMaterial, position) {
  const mesh = new THREE.Mesh(geometry, meshMaterial);
  mesh.position.set(...position);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

function buildRoom() {
  const room = new THREE.Group();

  // Wood floor.
  for (let i = 0; i < 26; ++i) {
    const z = -4.7 + i * 0.38;
    const color = [0x633920, 0x704227, 0x5b321e][i % 3];
    addMesh(
      room,
      new THREE.BoxGeometry(12, 0.07, 0.35),
      standardMaterial(color, 0.80),
      [0, -0.035, z]
    );
  }

  // Corner walls.
  addMesh(
    room,
    new THREE.BoxGeometry(12, 6.1, 0.16),
    standardMaterial(0x5c211d, 0.94),
    [0, 3, -4.65]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.16, 6.1, 9.3),
    standardMaterial(0x45171a, 0.95),
    [-6, 3, 0]
  );

  // Baseboards and upper trim.
  addMesh(
    room,
    new THREE.BoxGeometry(12, 0.25, 0.12),
    standardMaterial(0xc5ae79, 0.82),
    [0, 1.03, -4.54]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.12, 0.25, 9.1),
    standardMaterial(0xc5ae79, 0.82),
    [-5.90, 1.03, 0]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(12, 0.14, 0.12),
    standardMaterial(0x8d7449, 0.86),
    [0, 5.82, -4.54]
  );
  addMesh(
    room,
    new THREE.BoxGeometry(0.12, 0.14, 9.1),
    standardMaterial(0x8d7449, 0.86),
    [-5.90, 5.82, 0]
  );

  // Table assembly tucked into the back-left corner so its circular edge disappears into the walls.
  addMesh(
    room,
    new THREE.CylinderGeometry(4.35, 4.35, 0.04, 80),
    standardMaterial(0x101f1b, 0.96),
    [-1.65, 0.025, -0.30]
  );
  addMesh(
    room,
    new THREE.CylinderGeometry(3.65, 3.65, 0.24, 80),
    standardMaterial(0x17352e, 0.78),
    [-2.35, 1.18, -1.00]
  );
  addMesh(
    room,
    new THREE.CylinderGeometry(0.70, 1.18, 1.05, 40),
    standardMaterial(0x2d2722, 0.90),
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

function enableShadows(root) {
  root.traverse((object) => {
    if (!object.isMesh) return;
    object.castShadow = true;
    object.receiveShadow = true;
  });
}

function addCrtScreenOverlay(scene, tv, screenMaterial) {
  const box = new THREE.Box3().setFromObject(tv);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());

  const width = size.x * 0.80;
  const height = width * 0.75;

  const screen = new THREE.Mesh(
    new THREE.PlaneGeometry(width, height),
    screenMaterial
  );

  screen.position.set(
    center.x - size.x * 0.005,
    center.y + size.y * 0.07,
    box.max.z - 0.02
  );
  screen.rotation.y = tv.rotation.y;
  screen.renderOrder = 10;
  screen.castShadow = false;
  screen.receiveShadow = false;
  scene.add(screen);
}

async function loadAssets(scene, screenMaterial) {
  const loader = new GLTFLoader();

  const [nesGltf, tvGltf, cartridgeGltf] = await Promise.all([
    loader.loadAsync("./assets/nes_console_and_controller.glb"),
    loader.loadAsync("./assets/sony_pvm-14l2_crt_tv.glb"),
    loader.loadAsync("./assets/nes_cartridge__super_mario_bros.glb"),
  ]);

  const nes = nesGltf.scene;
  fitModel(nes, 2.15);
  nes.rotation.set(0, 0, 0);
  placeByBottomCenter(nes, -0.25, TABLE_TOP_Y, -0.60);
  enableShadows(nes);
  scene.add(nes);

  const tv = tvGltf.scene;
  fitModel(tv, 3.05);
  tv.rotation.set(0, 0, 0);
  placeByBottomCenter(tv, -2.35, TABLE_TOP_Y, -2.20);
  enableShadows(tv);
  scene.add(tv);
  addCrtScreenOverlay(scene, tv, screenMaterial);

  const cartridge = cartridgeGltf.scene;
  fitModel(cartridge, 0.98);
  cartridge.rotation.set(-0.04, 0, 0);
  placeByBottomCenter(cartridge, -4.60, TABLE_TOP_Y, -0.60);
  enableShadows(cartridge);
  scene.add(cartridge);

  return { cartridge };
}

export async function createScene(stage, nesCanvas, onCartridgeClick) {
  const renderer = new THREE.WebGPURenderer({ antialias: true });
  await renderer.init();

  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.setSize(stage.clientWidth, stage.clientHeight);
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.28;
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

  scene.add(new THREE.HemisphereLight(0xffe1bd, 0x3a2b28, 1.25));

  const warmLight = new THREE.PointLight(0xffc07a, 72, 18, 2);
  warmLight.position.set(0.0, 3.4, 5.8);
  warmLight.castShadow = true;
  warmLight.shadow.mapSize.set(1024, 1024);
  scene.add(warmLight);

  const fillLight = new THREE.PointLight(0xa9baf0, 18, 10, 2);
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
  });

  return {
    render() {
      nesTexture.needsUpdate = true;
      controls.update();
      renderer.render(scene, camera);
    },
    renderer,
    assetsLoaded,
  };
}
