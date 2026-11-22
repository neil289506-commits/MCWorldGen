
(()=>{
const $=(s,r=document)=>r.querySelector(s),$$=(s,r=document)=>[...r.querySelectorAll(s)];
// reveal on scroll
const io=new IntersectionObserver(es=>es.forEach(e=>{if(e.isIntersecting){e.target.classList.add('in');io.unobserve(e.target)}}),{threshold:.12});
$$('.reveal').forEach((el,i)=>{el.style.transitionDelay=(i%4)*70+'ms';io.observe(el)});
// cursor glow + card spotlight + tilt
const glow=$('.glow');
addEventListener('pointermove',e=>{if(glow){glow.style.left=e.clientX+'px';glow.style.top=e.clientY+'px'}});
$$('.card').forEach(c=>{
 c.addEventListener('pointermove',e=>{const r=c.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top;
  c.style.setProperty('--mx',x+'px');c.style.setProperty('--my',y+'px');
  c.style.transform=`perspective(800px) rotateX(${((y/r.height)-.5)*-8}deg) rotateY(${((x/r.width)-.5)*8}deg) translateY(-4px)`});
 c.addEventListener('pointerleave',()=>c.style.transform='');
});
// counters
$$('[data-count]').forEach(el=>{const t=+el.dataset.count;let n=0;const go=()=>{n+=Math.max(1,t/40);if(n>=t){el.textContent=t+(el.dataset.suf||'');return}el.textContent=Math.floor(n)+(el.dataset.suf||'');requestAnimationFrame(go)};
 new IntersectionObserver((es,o)=>{if(es[0].isIntersecting){go();o.disconnect()}}).observe(el)});
// toast helper
window.toast=m=>{let t=$('.toast');if(!t){t=document.createElement('div');t.className='toast';document.body.append(t)}t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2200)};
// background canvas
const cv=$('#bg');if(!cv)return;const ctx=cv.getContext('2d');let W,H,mode=cv.dataset.mode,col=getComputedStyle(document.documentElement).getPropertyValue('--a').trim(),col2=getComputedStyle(document.documentElement).getPropertyValue('--a2').trim();
const rs=()=>{W=cv.width=innerWidth;H=cv.height=innerHeight};rs();addEventListener('resize',rs);
let mx=.5,my=.5;addEventListener('pointermove',e=>{mx=e.clientX/W;my=e.clientY/H});
const P=[];
if(mode==='aurora'||mode==='nodes'){for(let i=0;i<70;i++)P.push({x:Math.random()*W,y:Math.random()*H,vx:(Math.random()-.5)*.5,vy:(Math.random()-.5)*.5,r:Math.random()*2+1})}
const drops=[];const cw=18;
const chars='01abcdef0123456789SHA512';
function frame(t){
 ctx.clearRect(0,0,W,H);
 if(mode==='aurora'){
  for(let k=0;k<3;k++){const g=ctx.createRadialGradient(W*(.2+.3*k+.1*Math.sin(t/3000+k)+(mx-.5)*.1),H*(.3+.2*Math.cos(t/2500+k)),0,W*(.2+.3*k),H*.4,W*.45);
   g.addColorStop(0,[col,col2,'#a855f7'][k]+'40');g.addColorStop(1,'transparent');ctx.fillStyle=g;ctx.fillRect(0,0,W,H)}
  ctx.fillStyle=col;P.forEach(p=>{p.x+=p.vx;p.y+=p.vy;if(p.x<0||p.x>W)p.vx*=-1;if(p.y<0||p.y>H)p.vy*=-1;ctx.globalAlpha=.5;ctx.beginPath();ctx.arc(p.x,p.y,p.r,0,7);ctx.fill()});
  ctx.globalAlpha=.12;ctx.strokeStyle=col;for(let i=0;i<P.length;i++)for(let j=i+1;j<P.length;j++){const dx=P[i].x-P[j].x,dy=P[i].y-P[j].y;if(dx*dx+dy*dy<14000){ctx.beginPath();ctx.moveTo(P[i].x,P[i].y);ctx.lineTo(P[j].x,P[j].y);ctx.stroke()}}ctx.globalAlpha=1;
 }else if(mode==='matrix'){
  const cols=Math.ceil(W/cw);while(drops.length<cols)drops.push(Math.random()*-50);
  ctx.fillStyle='rgba(0,0,0,.0)';ctx.font='15px monospace';
  drops.forEach((d,i)=>{const y=d*cw;ctx.fillStyle=col;ctx.globalAlpha=.55;ctx.fillText(chars[(Math.random()*chars.length)|0],i*cw,y);
   ctx.fillStyle=col2;ctx.globalAlpha=.18;for(let k=1;k<12;k++)ctx.fillText(chars[(i*7+k+(t/200|0))%chars.length],i*cw,y-k*cw);
   if(t%2<1)drops[i]+=.12+((i*13)%7)/60;if(y>H+200&&Math.random()>.97)drops[i]=Math.random()*-20});ctx.globalAlpha=1;
 }else if(mode==='grid'){
  const hz=H*.55;ctx.strokeStyle=col;ctx.globalAlpha=.25;ctx.lineWidth=1;
  for(let i=-30;i<=30;i++){ctx.beginPath();ctx.moveTo(W/2+i*20+(mx-.5)*200,hz);ctx.lineTo(W/2+i*160,H);ctx.stroke()}
  const off=(t/40)%1;for(let j=0;j<18;j++){const k=(j+off)/18,y=hz+(H-hz)*k*k;ctx.globalAlpha=.1+.3*k;ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(W,y);ctx.stroke()}
  const g=ctx.createRadialGradient(W/2,hz,0,W/2,hz,W*.5);g.addColorStop(0,col2+'55');g.addColorStop(1,'transparent');ctx.globalAlpha=1;ctx.fillStyle=g;ctx.fillRect(0,0,W,H);
 }else if(mode==='voxel'){
  if(!P.length){for(let i=0;i<46;i++)P.push({x:Math.random()*W,y:Math.random()*H,s:8+Math.random()*26,v:.1+Math.random()*.4,r:Math.random()*6,c:Math.random()})}
  P.forEach(p=>{p.y-=p.v;p.r+=.004;if(p.y<-60){p.y=H+60;p.x=Math.random()*W}
   ctx.save();ctx.translate(p.x,p.y);ctx.rotate(p.r);ctx.globalAlpha=.16;ctx.fillStyle=p.c>.5?col:col2;ctx.fillRect(-p.s/2,-p.s/2,p.s,p.s);ctx.strokeStyle=col;ctx.globalAlpha=.4;ctx.strokeRect(-p.s/2,-p.s/2,p.s,p.s);ctx.restore()});
 }
 requestAnimationFrame(frame)}
requestAnimationFrame(frame);
})();
