import * as THREE from "three/webgpu";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";

function material(color, roughness=0.7, metalness=0.0) {
  return new THREE.MeshStandardMaterial({ color, roughness, metalness });
}

function mesh(geometry, mat, position, rotation=[0,0,0]) {
  const object = new THREE.Mesh(geometry, mat);
  object.position.set(...position);
  object.rotation.set(...rotation);
  object.castShadow = true;
  object.receiveShadow = true;
  return object;
}

function buildFallbackRoom(screenMaterial) {
  const room = new THREE.Group();

  room.add(mesh(new THREE.BoxGeometry(12,0.2,9), material(0x5b4030), [0,-0.1,0]));
  room.add(mesh(new THREE.BoxGeometry(12,6,0.2), material(0x6b6258), [0,3,-4.4]));
  room.add(mesh(new THREE.BoxGeometry(0.2,6,9), material(0x5c554e), [-5.9,3,0]));

  const desk = mesh(new THREE.BoxGeometry(5.7,0.25,2.2), material(0x5a321d), [0,1.6,-1.4]);
  room.add(desk);
  room.add(mesh(new THREE.BoxGeometry(0.25,1.65,0.25), material(0x3d2418), [-2.45,0.75,-2.05]));
  room.add(mesh(new THREE.BoxGeometry(0.25,1.65,0.25), material(0x3d2418), [2.45,0.75,-2.05]));
  room.add(mesh(new THREE.BoxGeometry(0.25,1.65,0.25), material(0x3d2418), [-2.45,0.75,-0.75]));
  room.add(mesh(new THREE.BoxGeometry(0.25,1.65,0.25), material(0x3d2418), [2.45,0.75,-0.75]));

  const tv = new THREE.Group();
  tv.position.set(0,2.65,-1.55);
  tv.add(mesh(new THREE.BoxGeometry(3.15,2.45,1.75), material(0x292725,0.9), [0,0,0]));
  const bezel = mesh(new THREE.BoxGeometry(2.65,1.95,0.10), material(0x111111), [0,0,0.91]);
  tv.add(bezel);
  const screen = mesh(new THREE.PlaneGeometry(2.38,1.78), screenMaterial, [0,0,0.97]);
  screen.name = "CRT_SCREEN";
  tv.add(screen);
  room.add(tv);

  const consoleBody = mesh(new THREE.BoxGeometry(1.9,0.42,1.35), material(0xb8b8b0), [-1.3,1.95,-0.2]);
  room.add(consoleBody);
  room.add(mesh(new THREE.BoxGeometry(1.45,0.08,0.62), material(0x3a3a38), [-1.3,2.18,-0.28]));
  room.add(mesh(new THREE.BoxGeometry(0.15,0.09,0.18), material(0xb52222), [-2.05,2.18,0.15]));

  const controller = new THREE.Group();
  controller.position.set(1.25,1.9,-0.05);
  controller.rotation.x = -0.12;
  controller.add(mesh(new THREE.BoxGeometry(1.65,0.18,0.72), material(0xb9b9b4), [0,0,0]));
  controller.add(mesh(new THREE.BoxGeometry(0.48,0.08,0.16), material(0x292929), [-0.47,0.13,0]));
  controller.add(mesh(new THREE.BoxGeometry(0.16,0.08,0.48), material(0x292929), [-0.47,0.13,0]));
  controller.add(mesh(new THREE.CylinderGeometry(0.12,0.12,0.07,24), material(0xb11d2a), [0.48,0.13,-0.12],[Math.PI/2,0,0]));
  controller.add(mesh(new THREE.CylinderGeometry(0.12,0.12,0.07,24), material(0xb11d2a), [0.72,0.13,0.10],[Math.PI/2,0,0]));
  room.add(controller);

  return room;
}

function findScreen(root) {
  const candidates = [];
  root.traverse((object) => {
    if (!object.isMesh) return;
    const name = object.name.toLowerCase();
    if (/(screen|display|glass|crt|television|tv)/.test(name)) candidates.push(object);
  });
  return candidates.find((object) => /(screen|display)/.test(object.name.toLowerCase())) ?? candidates[0] ?? null;
}

function fitCamera(camera, controls, object) {
  const box = new THREE.Box3().setFromObject(object);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());
  const distance = Math.max(size.x,size.y,size.z) * 0.85;
  camera.position.set(center.x + distance*0.65, center.y + distance*0.38, center.z + distance);
  controls.target.copy(center);
  controls.update();
}

export async function createScene(stage, nesCanvas) {
  const renderer = new THREE.WebGPURenderer({ antialias:true });
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));
  renderer.setSize(stage.clientWidth,stage.clientHeight);
  renderer.shadowMap.enabled = true;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;
  stage.appendChild(renderer.domElement);

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x080706);
  scene.fog = new THREE.Fog(0x080706,12,26);

  const camera = new THREE.PerspectiveCamera(45,stage.clientWidth/stage.clientHeight,0.05,100);
  camera.position.set(7,5.4,8);

  const controls = new OrbitControls(camera,renderer.domElement);
  controls.enableDamping = true;
  controls.target.set(0,2,-1.2);
  controls.minDistance = 3;
  controls.maxDistance = 18;

  scene.add(new THREE.HemisphereLight(0xe5c39d,0x27221d,1.4));
  const lamp = new THREE.PointLight(0xffb96f,55,18,2);
  lamp.position.set(3.2,5,2.4);
  lamp.castShadow = true;
  scene.add(lamp);

  const nesTexture = new THREE.CanvasTexture(nesCanvas);
  nesTexture.colorSpace = THREE.SRGBColorSpace;
  nesTexture.magFilter = THREE.NearestFilter;
  nesTexture.minFilter = THREE.NearestFilter;
  nesTexture.generateMipmaps = false;

  const screenMaterial = new THREE.MeshBasicMaterial({ map:nesTexture, toneMapped:false });

  let loadedExternalScene = false;
  try {
    const gltf = await new GLTFLoader().loadAsync("./assets/nes-room.glb");
    const imported = gltf.scene;
    const screen = findScreen(imported);
    if (screen) {
      screen.material = screenMaterial;
      screen.material.needsUpdate = true;
      scene.add(imported);
      fitCamera(camera,controls,imported);
      loadedExternalScene = true;
      console.info("Loaded nes-room.glb; CRT candidate:",screen.name || screen.uuid);
    } else {
      console.warn("nes-room.glb loaded but no CRT screen mesh could be identified; using fallback room.");
    }
  } catch {
    // The downloadable CC BY room is optional. The generated room keeps this branch runnable.
  }

  if (!loadedExternalScene) scene.add(buildFallbackRoom(screenMaterial));

  const resize = () => {
    const width=stage.clientWidth,height=stage.clientHeight;
    camera.aspect=width/height;
    camera.updateProjectionMatrix();
    renderer.setSize(width,height);
  };
  addEventListener("resize",resize);

  return {
    render() {
      nesTexture.needsUpdate = true;
      controls.update();
      renderer.render(scene,camera);
    },
    renderer,
    loadedExternalScene,
  };
}
