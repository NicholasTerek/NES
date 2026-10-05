import createNesModule from "./nes.js";
import { createScene } from "./scene.js";

const APP_START=performance.now();
const DEBUG=new URLSearchParams(location.search).get("debug")==="1";
const WIDTH=256,HEIGHT=240;
const NES_FRAME_MS=1000/60.0988;
const palette=[
[84,84,84],[0,30,116],[8,16,144],[48,0,136],[68,0,100],[92,0,48],[84,4,0],[60,24,0],
[32,42,0],[8,58,0],[0,64,0],[0,60,0],[0,50,60],[0,0,0],[0,0,0],[0,0,0],
[152,150,152],[8,76,196],[48,50,236],[92,30,228],[136,20,176],[160,20,100],[152,34,32],[120,60,0],
[84,90,0],[40,114,0],[8,124,0],[0,118,40],[0,102,120],[0,0,0],[0,0,0],[0,0,0],
[236,238,236],[76,154,236],[120,124,236],[176,98,236],[228,84,236],[236,88,180],[236,106,100],[212,136,32],
[160,170,0],[116,196,0],[76,208,32],[56,204,108],[56,180,204],[60,60,60],[0,0,0],[0,0,0],
[236,238,236],[168,204,236],[188,188,236],[212,178,236],[236,174,236],[236,174,212],[236,180,176],[228,196,144],
[204,210,120],[180,222,120],[168,226,144],[152,226,180],[160,214,228],[160,162,160],[0,0,0],[0,0,0]
];
const keys=new Map([["KeyX",1],["KeyZ",2],["ShiftRight",4],["Enter",8],["ArrowUp",16],["ArrowDown",32],["ArrowLeft",64],["ArrowRight",128]]);

const nesCanvas=document.createElement("canvas");
nesCanvas.width=WIDTH; nesCanvas.height=HEIGHT;
const ctx=nesCanvas.getContext("2d",{alpha:false});
const image=ctx.createImageData(WIDTH,HEIGHT);
ctx.fillStyle="#050505"; ctx.fillRect(0,0,WIDTH,HEIGHT);

const stage=document.querySelector("#stage");
const romInput=document.querySelector("#rom");
const loadRomButton=document.querySelector("#load-rom");
const resetButton=document.querySelector("#reset");
const status=document.querySelector("#status");
const infoToggle=document.querySelector("#info-toggle");
const infoPanel=document.querySelector("#info-panel");
let debugPanel=null;
if(DEBUG){
  debugPanel=document.createElement("pre");
  debugPanel.className="perf-debug";
  debugPanel.textContent="Collecting performance data…";
  document.body.appendChild(debugPanel);
}

async function openRomPicker(){
  if(!module)return;
  try{await ensureAudio();}catch(error){console.warn("Audio unavailable:",error);}
  romInput.value="";
  romInput.click();
}

const view=await createScene(stage,nesCanvas,openRomPicker);

infoToggle.addEventListener("click",()=>{
  const hidden=infoPanel.classList.toggle("hidden");
  infoToggle.setAttribute("aria-expanded",String(!hidden));
});

let module,loaded=false,controller=0;
let audioContext=null,audioCursor=0;
const call=(name,returnType,argTypes=[],args=[])=>module.ccall(name,returnType,argTypes,args);

async function ensureAudio(){
  if(!audioContext){
    audioContext=new AudioContext();
    audioCursor=audioContext.currentTime;
  }
  if(audioContext.state==="suspended")await audioContext.resume();
}

function setKey(code,pressed){
  const bit=keys.get(code); if(bit===undefined||!module)return;
  controller=pressed?(controller|bit):(controller&~bit);
  call("nes_set_controller",null,["number"],[controller]);
}

function queueAudioFrame(){
  if(!audioContext||audioContext.state!=="running")return;
  const ptr=call("nes_audio_samples","number");
  const count=call("nes_audio_samples_size","number");
  if(!ptr||!count)return;

  const samples=module.HEAPF32.slice(ptr>>2,(ptr>>2)+count);
  const buffer=audioContext.createBuffer(1,count,44100);
  buffer.copyToChannel(samples,0);

  const source=audioContext.createBufferSource();
  source.buffer=buffer;
  source.connect(audioContext.destination);

  const now=audioContext.currentTime;
  if(audioCursor<now+0.03)audioCursor=now+0.03;
  source.start(audioCursor);
  audioCursor+=buffer.duration;
}

function runNesFrame(){
  if(!loaded)return false;
  call("nes_run_frame",null);
  queueAudioFrame();
  const ptr=call("nes_framebuffer","number");
  const size=call("nes_framebuffer_size","number");
  if(!ptr||size!==WIDTH*HEIGHT)return;
  const frame=module.HEAPU8.subarray(ptr,ptr+size);
  for(let i=0;i<frame.length;i++){
    const [r,g,b]=palette[frame[i]&0x3f],o=i*4;
    image.data[o]=r;image.data[o+1]=g;image.data[o+2]=b;image.data[o+3]=255;
  }
  ctx.putImageData(image,0,0);
  return true;
}

addEventListener("keydown",e=>{if(keys.has(e.code)){e.preventDefault();setKey(e.code,true);}});
addEventListener("keyup",e=>{if(keys.has(e.code)){e.preventDefault();setKey(e.code,false);}});

loadRomButton.addEventListener("click",openRomPicker);

romInput.addEventListener("change",async()=>{
  const file=romInput.files?.[0]; if(!file||!module)return;
  try{
    const bytes=new Uint8Array(await file.arrayBuffer());
    const ptr=module._malloc(bytes.length);
    try{
      module.HEAPU8.set(bytes,ptr);
      loaded=call("nes_load_rom","number",["number","number"],[ptr,bytes.length])===1;
    }finally{
      module._free(ptr);
    }
    resetButton.disabled=!loaded;
    status.textContent=loaded?`Playing ${file.name}`:`Could not load ${file.name}`;
    if(loaded)emulationAccumulator=0;
  }catch(error){
    loaded=false;
    resetButton.disabled=true;
    status.textContent=`ROM load failed: ${error?.message ?? error}`;
    console.error("ROM load failed:",error);
  }
});
resetButton.addEventListener("click",async()=>{
  if(!loaded)return;
  try{await ensureAudio();}catch(error){console.warn("Audio unavailable:",error);}
  call("nes_reset",null);
  if(audioContext)audioCursor=audioContext.currentTime;
  emulationAccumulator=0;
});

try{
  module=await createNesModule({
    locateFile:path=>new URL(path,import.meta.url).href,
  });
  loadRomButton.disabled=false;
  status.textContent=view.assetsLoaded?"Ready · click the cartridge or Load ROM":"Ready · room loaded; asset load failed";
  requestAnimationFrame(()=>document.body.classList.add("ready"));
}catch(error){
  status.textContent=`Emulator failed to initialize: ${error?.message ?? error}`;
  document.querySelector(".loading-label").textContent="Could not start emulator";
  console.error("Emulator initialization failed:",error);
  throw error;
}
let lastTick=performance.now();
let emulationAccumulator=0;
let debugLastUpdate=lastTick;
let debugCallbacks=0;
let debugEmulatedFrames=0;
let debugRenderedFrames=0;
let debugWindowStart=lastTick;
const debugFrameTimes=[];

function updateDebugPanel(now){
  if(!DEBUG||!debugPanel||now-debugLastUpdate<1000)return;
  const elapsedSeconds=(now-debugWindowStart)/1000;
  const sorted=[...debugFrameTimes].sort((a,b)=>a-b);
  const p95=sorted.length?sorted[Math.min(sorted.length-1,Math.floor(sorted.length*0.95))]:0;
  const average=debugFrameTimes.length?debugFrameTimes.reduce((sum,value)=>sum+value,0)/debugFrameTimes.length:0;
  const stats=view.getDebugStats();
  const heap=performance.memory
    ? `${(performance.memory.usedJSHeapSize/1048576).toFixed(1)} / ${(performance.memory.jsHeapSizeLimit/1048576).toFixed(0)} MB`
    : "n/a";
  debugPanel.textContent=[
    "NES PERF",
    `callback FPS   ${(debugCallbacks/elapsedSeconds).toFixed(1)}`,
    `emulation FPS  ${(debugEmulatedFrames/elapsedSeconds).toFixed(1)}`,
    `render FPS     ${(debugRenderedFrames/elapsedSeconds).toFixed(1)}`,
    `frame avg/p95  ${average.toFixed(2)} / ${p95.toFixed(2)} ms`,
    `pixel ratio    ${stats.pixelRatio.toFixed(2)}`,
    `draw calls     ${stats.calls}`,
    `triangles      ${stats.triangles.toLocaleString()}`,
    `geometries     ${stats.geometries}`,
    `textures       ${stats.textures}`,
    `JS heap        ${heap}`,
    `load-to-ready  ${(performance.now()-APP_START).toFixed(0)} ms`,
    `total renders  ${stats.renders}`,
  ].join("\n");
  console.debug("[NES perf]",{
    callbackFps:debugCallbacks/elapsedSeconds,
    emulationFps:debugEmulatedFrames/elapsedSeconds,
    renderFps:debugRenderedFrames/elapsedSeconds,
    frameAverageMs:average,
    frameP95Ms:p95,
    ...stats,
  });
  debugCallbacks=0;
  debugEmulatedFrames=0;
  debugRenderedFrames=0;
  debugFrameTimes.length=0;
  debugWindowStart=now;
  debugLastUpdate=now;
}

view.renderer.setAnimationLoop((now)=>{
  const rawElapsed=now-lastTick;
  const elapsed=Math.min(rawElapsed,100);
  lastTick=now;
  emulationAccumulator+=elapsed;
  if(DEBUG){
    debugCallbacks+=1;
    if(rawElapsed>0&&rawElapsed<250){
      debugFrameTimes.push(rawElapsed);
      if(debugFrameTimes.length>240)debugFrameTimes.shift();
    }
  }

  let frameUpdated=false;
  let catchUpFrames=0;
  while(loaded&&emulationAccumulator>=NES_FRAME_MS&&catchUpFrames<3){
    frameUpdated=runNesFrame()||frameUpdated;
    emulationAccumulator-=NES_FRAME_MS;
    catchUpFrames+=1;
    if(DEBUG)debugEmulatedFrames+=1;
  }
  if(catchUpFrames===3&&emulationAccumulator>=NES_FRAME_MS){
    emulationAccumulator=0;
  }

  const rendered=view.render(frameUpdated);
  if(DEBUG&&rendered)debugRenderedFrames+=1;
  updateDebugPanel(now);
});
