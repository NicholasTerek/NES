import * as THREE from "three/webgpu";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { RoundedBoxGeometry } from "three/addons/geometries/RoundedBoxGeometry.js";
import { GLTFLoader } from "three/addons/loaders/GLTFLoader.js";

function material(color, roughness=0.72, metalness=0.0) {
  return new THREE.MeshStandardMaterial({ color, roughness, metalness });
}

function addMesh(parent, geometry, mat, position, rotation=[0,0,0]) {
  const object = new THREE.Mesh(geometry, mat);
  object.position.set(...position);
  object.rotation.set(...rotation);
  object.castShadow = true;
  object.receiveShadow = true;
  parent.add(object);
  return object;
}

function roundedBox(width, height, depth, radius=0.08, segments=4) {
  return new RoundedBoxGeometry(width, height, depth, segments, radius);
}

function makeCanvasTexture(draw, width=768, height=512) {
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext("2d");
  draw(ctx, width, height);
  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return texture;
}

function makeMapPoster() {
  return makeCanvasTexture((ctx,w,h) => {
    ctx.fillStyle="#b6a16e";
    ctx.fillRect(0,0,w,h);
    ctx.fillStyle="#35472f";
    for(let y=28;y<h-28;y+=24){
      for(let x=28;x<w-28;x+=24){
        const n=(x*13+y*29)%17;
        if(n<7) ctx.fillRect(x,y,19,19);
      }
    }
    ctx.strokeStyle="#6f3d28";
    ctx.lineWidth=14;
    ctx.strokeRect(7,7,w-14,h-14);
    ctx.fillStyle="#2f271d";
    ctx.font="bold 40px monospace";
    ctx.fillText("8-BIT WORLD",32,58);
  });
}

function makeBattlePoster() {
  return makeCanvasTexture((ctx,w,h) => {
    const gradient=ctx.createLinearGradient(0,0,w,h);
    gradient.addColorStop(0,"#33205d");
    gradient.addColorStop(.45,"#8b3429");
    gradient.addColorStop(1,"#d49336");
    ctx.fillStyle=gradient;
    ctx.fillRect(0,0,w,h);

    ctx.fillStyle="#17152a";
    ctx.beginPath(); ctx.arc(590,150,85,0,Math.PI*2); ctx.fill();

    ctx.strokeStyle="#f5d26f";
    ctx.lineWidth=16;
    ctx.beginPath(); ctx.moveTo(90,410); ctx.lineTo(220,90); ctx.lineTo(330,410); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(285,410); ctx.lineTo(430,105); ctx.lineTo(535,410); ctx.stroke();

    ctx.fillStyle="#f2d36f";
    ctx.font="bold 58px sans-serif";
    ctx.fillText("RETRO",42,470);
    ctx.fillStyle="#f5eee5";
    ctx.font="bold 34px sans-serif";
    ctx.fillText("BATTLE",565,462);
  });
}

function addPoster(parent, texture, position, rotation=[0,0,0], size=[2.8,1.85]) {
  const frame=addMesh(
    parent,
    roundedBox(size[0]+0.12,size[1]+0.12,0.08,0.03,3),
    material(0x231715,.62),
    position,
    rotation
  );

  const poster=new THREE.Mesh(
    new THREE.PlaneGeometry(size[0],size[1]),
    new THREE.MeshBasicMaterial({ map:texture, toneMapped:false })
  );
  poster.position.copy(frame.position);
  poster.rotation.copy(frame.rotation);

  const offset=new THREE.Vector3(0,0,0.045).applyEuler(poster.rotation);
  poster.position.add(offset);
  parent.add(poster);
}

function addCable(parent, points, radius=0.025) {
  const curve=new THREE.CatmullRomCurve3(points.map((point)=>new THREE.Vector3(...point)));
  const cable=addMesh(
    parent,
    new THREE.TubeGeometry(curve,72,radius,8,false),
    material(0x101010,.92),
    [0,0,0]
  );
  cable.castShadow=false;
  return cable;
}

function addCartridge(parent, position, rotation=[0,0,0], accent=0x8e2e24) {
  const group=new THREE.Group();
  group.position.set(...position);
  group.rotation.set(...rotation);

  addMesh(group,roundedBox(.72,.09,.95,.045,3),material(0xc2b79a,.82),[0,0,0]);
  addMesh(group,new THREE.BoxGeometry(.46,.012,.44),material(accent,.72),[0,.053,-.05]);
  addMesh(group,new THREE.BoxGeometry(.50,.012,.08),material(0x28231e,.82),[0,.054,.28]);
  parent.add(group);
  return group;
}

function addPokeballLikeObject(parent) {
  const group=new THREE.Group();
  group.position.set(3.35,1.62,-.20);

  const top=addMesh(group,new THREE.SphereGeometry(.34,36,20,0,Math.PI*2,0,Math.PI/2),material(0xa92a28,.42),[0,0,0]);
  const bottom=addMesh(group,new THREE.SphereGeometry(.34,36,20,0,Math.PI*2,Math.PI/2,Math.PI/2),material(0xdad7ce,.55),[0,0,0]);
  top.rotation.x=0;
  bottom.rotation.x=0;

  addMesh(group,new THREE.CylinderGeometry(.345,.345,.055,36),material(0x161616,.78),[0,0,0],[Math.PI/2,0,0]);
  addMesh(group,new THREE.CylinderGeometry(.095,.095,.065,24),material(0xe1ddd3,.45),[0,0,.345],[Math.PI/2,0,0]);
  addMesh(group,new THREE.CylinderGeometry(.14,.14,.045,24),material(0x151515,.70),[0,0,.322],[Math.PI/2,0,0]);

  parent.add(group);
}

function buildRoom(screenMaterial) {
  const room=new THREE.Group();

  // Wood floor using many planks.
  for(let i=0;i<26;i++){
    const z=-4.7+i*.38;
    const tone=[0x633920,0x704227,0x5b321e][i%3];
    addMesh(room,new THREE.BoxGeometry(12,.07,.35),material(tone,.80),[0,-.035,z]);
  }

  // Red corner room.
  addMesh(room,new THREE.BoxGeometry(12,6.1,.16),material(0x5c211d,.94),[0,3,-4.65]);
  addMesh(room,new THREE.BoxGeometry(.16,6.1,9.3),material(0x45171a,.95),[-6,3,0]);

  // Baseboards and ceiling trim.
  addMesh(room,new THREE.BoxGeometry(12,.25,.12),material(0xc5ae79,.82),[0,1.03,-4.54]);
  addMesh(room,new THREE.BoxGeometry(.12,.25,9.1),material(0xc5ae79,.82),[-5.90,1.03,0]);
  addMesh(room,new THREE.BoxGeometry(12,.14,.12),material(0x8d7449,.86),[0,5.82,-4.54]);
  addMesh(room,new THREE.BoxGeometry(.12,.14,9.1),material(0x8d7449,.86),[-5.90,5.82,0]);

  // Dark circular area and table, matching the reference composition.
  addMesh(room,new THREE.CylinderGeometry(4.35,4.35,.04,80),material(0x101f1b,.96),[0,.025,.40]);
  addMesh(room,new THREE.CylinderGeometry(3.65,3.65,.24,80),material(0x17352e,.78),[0,1.18,.35]);
  addMesh(room,new THREE.CylinderGeometry(.70,1.18,1.05,40),material(0x2d2722,.90),[0,.62,.35]);

  // Posters.
  addPoster(room,makeMapPoster(),[-3.45,3.90,-4.54],[0,0,0],[3.15,2.05]);
  addPoster(room,makeBattlePoster(),[2.92,4.24,-4.54],[0,0,0],[3.05,2.00]);


  // Magazine/comic style object on the front right.
  const magazineTexture=makeCanvasTexture((ctx,w,h)=>{
    ctx.fillStyle="#bfc35e";ctx.fillRect(0,0,w,h);
    ctx.fillStyle="#7c2c22";ctx.beginPath();ctx.arc(210,300,95,0,Math.PI*2);ctx.fill();
    ctx.fillStyle="#284a31";ctx.fillRect(310,120,250,260);
    ctx.fillStyle="#ece4b1";ctx.font="bold 72px sans-serif";ctx.fillText("SUPER",50,95);
    ctx.fillStyle="#ece4b1";ctx.font="bold 56px sans-serif";ctx.fillText("8-BIT",55,160);
  });
  const magazine=addMesh(
    room,
    new THREE.PlaneGeometry(1.7,1.18),
    new THREE.MeshBasicMaterial({map:magazineTexture,toneMapped:false}),
    [2.10,1.325,2.28],
    [-Math.PI/2,-.08,-.24]
  );
  magazine.castShadow=false;

  addPokeballLikeObject(room);

  // Small Pac-Man-like wall light above TV.
  const glow=new THREE.MeshBasicMaterial({color:0xffbe3e,toneMapped:false});
  const pac=addMesh(room,new THREE.SphereGeometry(.17,28,18),glow,[.72,4.72,-4.43]);
  pac.scale.set(1,1,.25);

  return room;
}

function fitModel(root, targetSize) {
  const box = new THREE.Box3().setFromObject(root);
  const size = box.getSize(new THREE.Vector3());
  const maxAxis = Math.max(size.x, size.y, size.z);
  if (maxAxis > 0) {
    const scale = targetSize / maxAxis;
    root.scale.multiplyScalar(scale);
  }

  const fitted = new THREE.Box3().setFromObject(root);
  const center = fitted.getCenter(new THREE.Vector3());
  root.position.sub(center);
}

function placeByBottomCenter(root, x, y, z) {
  const box = new THREE.Box3().setFromObject(root);
  const center = box.getCenter(new THREE.Vector3());
  root.position.x += x - center.x;
  root.position.y += y - box.min.y;
  root.position.z += z - center.z;
}

function addCrtScreenOverlay(scene, tv, screenMaterial) {
  const box = new THREE.Box3().setFromObject(tv);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());

  const width = size.x * 0.62;
  const height = width * 0.75;

  const screen = new THREE.Mesh(
    new THREE.PlaneGeometry(width, height),
    screenMaterial
  );

  screen.position.set(
    center.x - size.x * 0.055,
    center.y + size.y * 0.06,
    box.max.z + 0.018
  );
  screen.rotation.y = tv.rotation.y;
  screen.renderOrder = 10;
  screen.castShadow = false;
  screen.receiveShadow = false;
  scene.add(screen);
  return screen;
}

async function loadRealAssets(scene, screenMaterial) {
  const loader = new GLTFLoader();

  const [nesGltf, tvGltf, cartridgeGltf] = await Promise.all([
    loader.loadAsync("./assets/nes_console_and_controller.glb"),
    loader.loadAsync("./assets/sony_pvm-14l2_crt_tv.glb"),
    loader.loadAsync("./assets/nes_cartridge__super_mario_bros.glb"),
  ]);

  const nes = nesGltf.scene;
  fitModel(nes, 2.55);
  nes.rotation.y = 0.10;
  placeByBottomCenter(nes, -1.05, 1.31, 0.62);
  nes.traverse((object) => {
    if (object.isMesh) {
      object.castShadow = true;
      object.receiveShadow = true;
    }
  });
  scene.add(nes);

  const tv = tvGltf.scene;
  fitModel(tv, 3.65);
  tv.rotation.y = -0.035;
  placeByBottomCenter(tv, 0.70, 1.31, -1.12);
  tv.traverse((object) => {
    if (object.isMesh) {
      object.castShadow = true;
      object.receiveShadow = true;
    }
  });

  scene.add(tv);
  const screen = addCrtScreenOverlay(scene, tv, screenMaterial);

  const cartridge = cartridgeGltf.scene;
  fitModel(cartridge, 1.15);
  cartridge.rotation.set(-0.16, 0.34, 0.04);
  placeByBottomCenter(cartridge, -2.55, 1.31, 0.95);
  cartridge.userData.clickableCartridge = true;
  cartridge.traverse((object) => {
    if (object.isMesh) {
      object.castShadow = true;
      object.receiveShadow = true;
      object.userData.clickableCartridge = true;
    }
  });
  scene.add(cartridge);

  return { nes, tv, cartridge, screenFound: true };
}

export async function createScene(stage, nesCanvas, onCartridgeClick) {
  const renderer=new THREE.WebGPURenderer({antialias:true});
  await renderer.init();
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));
  renderer.setSize(stage.clientWidth,stage.clientHeight);
  renderer.shadowMap.enabled=true;
  renderer.shadowMap.type=THREE.PCFSoftShadowMap;
  renderer.toneMapping=THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure=1.28;
  stage.appendChild(renderer.domElement);

  const scene=new THREE.Scene();
  scene.background=new THREE.Color(0x080606);
  scene.fog=new THREE.Fog(0x080606,10.5,22);

  const camera=new THREE.PerspectiveCamera(46,stage.clientWidth/stage.clientHeight,.05,100);
  camera.position.set(7.35,4.65,7.70);

  const controls=new OrbitControls(camera,renderer.domElement);
  controls.enableDamping=true;
  controls.target.set(.20,2.15,-.40);
  controls.minDistance=3.3;
  controls.maxDistance=15;
  controls.maxPolarAngle=Math.PI*.57;

  const hemi=new THREE.HemisphereLight(0xffe1bd,0x3a2b28,1.45);
  scene.add(hemi);

  const warm=new THREE.PointLight(0xffb35c,88,15,2);
  warm.position.set(.75,4.78,-3.65);
  warm.castShadow=true;
  warm.shadow.mapSize.set(1024,1024);
  scene.add(warm);

  const fill=new THREE.PointLight(0xa9baf0,30,12,2);
  fill.position.set(-4.5,3.7,3.6);
  scene.add(fill);

  const nesTexture=new THREE.CanvasTexture(nesCanvas);
  nesTexture.colorSpace=THREE.SRGBColorSpace;
  nesTexture.magFilter=THREE.NearestFilter;
  nesTexture.minFilter=THREE.NearestFilter;
  nesTexture.generateMipmaps=false;

  const screenMaterial=new THREE.MeshBasicMaterial({
    map:nesTexture,
    toneMapped:false
  });

  scene.add(buildRoom(screenMaterial));

  let assetsLoaded = false;
  let cartridge = null;
  try {
    const assets = await loadRealAssets(scene, screenMaterial);
    cartridge = assets.cartridge;
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
    if (cartridgeHit(event)) {
      event.stopPropagation();
      onCartridgeClick?.();
    }
  });

  const resize=()=>{
    const width=stage.clientWidth;
    const height=stage.clientHeight;
    camera.aspect=width/height;
    camera.updateProjectionMatrix();
    renderer.setSize(width,height);
  };
  addEventListener("resize",resize);

  return {
    render(){
      nesTexture.needsUpdate=true;
      controls.update();
      renderer.render(scene,camera);
    },
    renderer,
    assetsLoaded,
  };
}
