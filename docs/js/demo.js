
function hash(s){let h=2166136261;for(const c of s){h^=c.charCodeAt(0);h=Math.imul(h,16777619)}return h>>>0}
function rng(seed){let a=seed;return()=>{a|=0;a=a+0x6D2B79F5|0;let t=Math.imul(a^a>>>15,1|a);t=t+Math.imul(t^t>>>7,61|t)^t;return((t^t>>>14)>>>0)/4294967296}}
function mkNoise(seed){const r=rng(seed),G=64,g=[...Array(G*G)].map(()=>r());
 const sm=t=>t*t*(3-2*t),v=(x,y)=>g[((y%G+G)%G)*G+((x%G+G)%G)];
 return(x,y)=>{const xi=Math.floor(x),yi=Math.floor(y),xf=sm(x-xi),yf=sm(y-yi);
  const a=v(xi,yi),b=v(xi+1,yi),c=v(xi,yi+1),d=v(xi+1,yi+1);return a+(b-a)*xf+(c-a)*yf+(a-b-c+d)*xf*yf}}
const cv=document.getElementById('map'),cx=cv.getContext('2d');let prog=-1,R=24;
function biome(e,m){if(e<.30)return[30,70,160];if(e<.36)return[70,120,200];if(e<.40)return[228,214,150];
 if(e>.78)return[245,248,250];if(e>.68)return[130,130,135];
 if(m<.35)return e<.5?[215,190,110]:[190,150,80];if(m>.68)return[30,110,50];return e>.6?[70,130,60]:[96,165,66]}
function draw(){const n1=mkNoise(hash(sd.value)),n2=mkNoise(hash(sd.value+'m')),S=120,im=cx.createImageData(S,S),sc=wt.value==='amplified'?.07:wt.value==='large biomes'?.025:.045;
 for(let y=0;y<S;y++)for(let x=0;x<S;x++){let e=n1(x*sc,y*sc)*.65+n1(x*sc*2.3,y*sc*2.3)*.25+n1(x*sc*5,y*sc*5)*.1;if(wt.value==='amplified')e=Math.pow(e,1.5)*1.6;
  const m=n2(x*sc*.8,y*sc*.8);let[r,g,b]=biome(e,m);const dx=x-S/2,dy=y-S/2,inR=Math.hypot(dx,dy)<R*S/96;
  if(prog>=0){const k=Math.hypot(dx,dy)<prog*R*S/96;if(!k){r*=.25;g*=.25;b*=.3}}else if(!inR){r*=.55;g*=.55;b*=.6}
  const i=(y*S+x)*4;im.data.set([r,g,b,255],i)}
 const t=document.createElement('canvas');t.width=t.height=S;t.getContext('2d').putImageData(im,0,0);cx.imageSmoothingEnabled=false;cx.drawImage(t,0,0,480,480);
 cx.strokeStyle='rgba(255,255,255,.6)';cx.setLineDash([6,5]);cx.beginPath();cx.arc(240,240,R*5,0,7);cx.stroke();cx.setLineDash([])}
[sd,wt,ld].forEach(e=>e.addEventListener('input',()=>{prog=-1;draw()}));rad.oninput=()=>{R=+rad.value;rv.textContent=R;prog=-1;draw()};
rnd.onclick=()=>{sd.value=Math.random().toString(36).slice(2,10);prog=-1;draw()};draw();
go.onclick=()=>{const L=[`[WorldGen] 安裝 ${ld.value} 伺服器核心 → %TEMP%\\worldgen-pregen-…`,'[WorldGen] 匯入 mods / datapack','[WorldGen] 寫入 server.properties (seed='+sd.value+')','[Server] Starting minecraft server …','[Server] Preparing spawn area …'];con.textContent='';let i=0;prog=0;
 const iv=setInterval(()=>{if(i<L.length){con.textContent+=L[i++]+'\n'}else{prog+=.04;con.textContent+=`[Pregen] ${Math.min(100,Math.round(prog*100))}% 已生成區塊\n`;if(prog>=1){con.textContent+='[Server] Done (12.3s)! 自動送出 stop\n[Export] 以 BZIP2 打包 → world.zip ✔\n';clearInterval(iv);toast('預生成完成並已匯出 zip')}}draw();con.scrollTop=1e5},220)};
