import * as THREE from "three/webgpu";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { RoundedBoxGeometry } from "three/addons/geometries/RoundedBoxGeometry.js";

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

function addController(parent) {
  const controller=new THREE.Group();
  controller.position.set(1.95,1.54,.34);
  controller.rotation.set(-.07,-.34,-.03);

  addMesh(controller,roundedBox(1.72,.22,.73,.08,4),material(0xbcbab3,.72),[0,0,0]);

  addMesh(controller,new THREE.BoxGeometry(.53,.08,.17),material(0x242424,.86),[-.48,.15,0]);
  addMesh(controller,new THREE.BoxGeometry(.17,.08,.53),material(0x242424,.86),[-.48,.15,0]);

  addMesh(controller,new THREE.CylinderGeometry(.12,.12,.075,24),material(0xa7252d,.55),[.50,.15,-.13],[Math.PI/2,0,0]);
  addMesh(controller,new THREE.CylinderGeometry(.12,.12,.075,24),material(0xa7252d,.55),[.73,.15,.08],[Math.PI/2,0,0]);

  addMesh(controller,new THREE.BoxGeometry(.24,.055,.08),material(0x4a4743,.75),[-.02,.15,.02]);
  addMesh(controller,new THREE.BoxGeometry(.24,.055,.08),material(0x4a4743,.75),[.27,.15,.02]);

  parent.add(controller);

  addCable(parent,[
    [2.58,1.50,.32],
    [2.95,1.44,.18],
    [2.68,1.33,-.18],
    [2.18,1.27,-.36],
    [1.40,1.31,-.22]
  ],.022);
}

function addConsole(parent) {
  const consoleGroup=new THREE.Group();
  consoleGroup.position.set(-1.18,1.58,.55);
  consoleGroup.rotation.y=.08;

  addMesh(consoleGroup,roundedBox(2.55,.60,1.72,.10,4),material(0xc3c1ba,.76),[0,0,0]);
  addMesh(consoleGroup,new THREE.BoxGeometry(2.02,.13,.99),material(0x3b3a37,.88),[.13,.36,-.16]);
  addMesh(consoleGroup,new THREE.BoxGeometry(1.86,.052,.54),material(0x161616,.95),[.15,.445,-.18]);

  addMesh(consoleGroup,new THREE.BoxGeometry(.38,.14,.18),material(0x2a2a28,.80),[-.92,-.03,.87]);
  addMesh(consoleGroup,new THREE.BoxGeometry(.38,.14,.18),material(0x2a2a28,.80),[-.46,-.03,.87]);

  addMesh(
    consoleGroup,
    new THREE.BoxGeometry(.08,.08,.028),
    new THREE.MeshBasicMaterial({color:0xff2e25,toneMapped:false}),
    [-1.08,.17,.875]
  );

  const label=makeCanvasTexture((ctx,w,h)=>{
    ctx.fillStyle="#d4d1c8"; ctx.fillRect(0,0,w,h);
    ctx.fillStyle="#9b2428"; ctx.font="bold 84px sans-serif"; ctx.fillText("8-BIT",40,110);
    ctx.fillStyle="#2a2a2a"; ctx.font="36px sans-serif"; ctx.fillText("ENTERTAINMENT SYSTEM",42,170);
  },512,220);

  const labelMesh=new THREE.Mesh(
    new THREE.PlaneGeometry(.90,.39),
    new THREE.MeshBasicMaterial({map:label,toneMapped:false})
  );
  labelMesh.position.set(-.63,.07,.873);
  consoleGroup.add(labelMesh);

  parent.add(consoleGroup);

  addCable(parent,[
    [-.10,1.52,.66],
    [.02,1.36,.90],
    [.22,1.25,1.12],
    [.65,1.24,1.20]
  ],.028);
}

function addCRT(parent, screenMaterial) {
  const tv=new THREE.Group();
  tv.position.set(.65,2.85,-1.12);
  tv.rotation.y=-.035;

  addMesh(tv,roundedBox(3.58,2.86,1.82,.24,7),material(0x171716,.72),[0,0,0]);
  addMesh(tv,roundedBox(2.89,2.27,.15,.18,6),material(0x2a2a26,.74),[-.18,.06,.92]);

  const glassMat=new THREE.MeshPhysicalMaterial({
    color:0x304143,
    roughness:.22,
    metalness:0,
    clearcoat:1,
    clearcoatRoughness:.10,
    transparent:true,
    opacity:.24
  });
  addMesh(tv,roundedBox(2.58,1.95,.08,.20,7),glassMat,[-.18,.06,1.02]);

  const screen=addMesh(tv,new THREE.PlaneGeometry(2.45,1.83),screenMaterial,[-.18,.06,1.067]);
  screen.name="CRT_SCREEN";

  // Speaker grille.
  for(let i=0;i<8;i++){
    addMesh(tv,new THREE.BoxGeometry(.04,.82,.025),material(0x080808,.96),[1.43,.19,1.00]);
    tv.children[tv.children.length-1].position.x=1.30+i*.055;
  }

  addMesh(tv,new THREE.CylinderGeometry(.115,.115,.085,28),material(0x333331,.64),[1.48,-.62,1.00],[Math.PI/2,0,0]);
  addMesh(tv,new THREE.CylinderGeometry(.090,.090,.085,28),material(0x333331,.64),[1.48,-.89,1.00],[Math.PI/2,0,0]);

  parent.add(tv);

  // Cable disappearing behind the TV.
  addCable(parent,[
    [1.42,1.65,-.72],
    [2.15,1.45,-.18],
    [2.30,1.26,.12],
    [1.98,1.18,.42]
  ],.028);

  return tv;
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

  addCRT(room,screenMaterial);
  addConsole(room);
  addController(room);

  addCartridge(room,[-3.05,1.42,.85],[-.05,.22,.10],0x8a3826);
  addCartridge(room,[-2.62,1.43,.66],[-.08,-.28,-.03],0x423e37);
  addCartridge(room,[.06,1.43,-.03],[-.05,.10,.04],0x5e2b25);

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

export async function createScene(stage, nesCanvas) {
  const renderer=new THREE.WebGPURenderer({antialias:true});
  await renderer.init();
  renderer.setPixelRatio(Math.min(devicePixelRatio,2));
  renderer.setSize(stage.clientWidth,stage.clientHeight);
  renderer.shadowMap.enabled=true;
  renderer.shadowMap.type=THREE.PCFSoftShadowMap;
  renderer.toneMapping=THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure=.90;
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

  const hemi=new THREE.HemisphereLight(0xcaa77c,0x171313,.65);
  scene.add(hemi);

  const warm=new THREE.PointLight(0xffa33b,58,13,2);
  warm.position.set(.75,4.78,-3.65);
  warm.castShadow=true;
  warm.shadow.mapSize.set(1024,1024);
  scene.add(warm);

  const fill=new THREE.PointLight(0x6e87c7,16,10,2);
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
  };
}
