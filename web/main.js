import createNesModule from "./nes.js";
import { createScene } from "./scene.js";

const WIDTH=256,HEIGHT=240;
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
const resetButton=document.querySelector("#reset");
const status=document.querySelector("#status");
const view=await createScene(stage,nesCanvas,()=>romInput.click());

let module,loaded=false,controller=0;
const call=(name,returnType,argTypes=[],args=[])=>module.ccall(name,returnType,argTypes,args);

function setKey(code,pressed){
  const bit=keys.get(code); if(bit===undefined||!module)return;
  controller=pressed?(controller|bit):(controller&~bit);
  call("nes_set_controller",null,["number"],[controller]);
}

function runNesFrame(){
  if(!loaded)return;
  call("nes_run_frame",null);
  const ptr=call("nes_framebuffer","number");
  const size=call("nes_framebuffer_size","number");
  if(!ptr||size!==WIDTH*HEIGHT)return;
  const frame=module.HEAPU8.subarray(ptr,ptr+size);
  for(let i=0;i<frame.length;i++){
    const [r,g,b]=palette[frame[i]&0x3f],o=i*4;
    image.data[o]=r;image.data[o+1]=g;image.data[o+2]=b;image.data[o+3]=255;
  }
  ctx.putImageData(image,0,0);
}

addEventListener("keydown",e=>{if(keys.has(e.code)){e.preventDefault();setKey(e.code,true);}});
addEventListener("keyup",e=>{if(keys.has(e.code)){e.preventDefault();setKey(e.code,false);}});

romInput.addEventListener("change",async()=>{
  const file=romInput.files?.[0]; if(!file)return;
  const bytes=new Uint8Array(await file.arrayBuffer());
  const ptr=module._malloc(bytes.length);
  try{
    module.HEAPU8.set(bytes,ptr);
    loaded=call("nes_load_rom","number",["number","number"],[ptr,bytes.length])===1;
    resetButton.disabled=!loaded;
    status.textContent=loaded?`Playing ${file.name}`:"Could not load this ROM.";
  }finally{module._free(ptr);}
});
resetButton.addEventListener("click",()=>{if(loaded)call("nes_reset",null);});

module=await createNesModule();
status.textContent=view.assetsLoaded?"Ready · click the cartridge to load a ROM":"Ready · room loaded; asset load failed";
view.renderer.setAnimationLoop(()=>{runNesFrame();view.render();});
