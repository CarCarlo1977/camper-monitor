/* Theme & UI Management */
function tT() {
  const d = document.getElementById('thT').checked;
  document.documentElement.setAttribute('data-theme', d ? 'light' : 'dark');
  try {
    localStorage.setItem('th', d ? 'light' : 'dark')
  } catch (e) {}
}

(function() {
  let t = 'dark';
  try {
    t = localStorage.getItem('th') || 'dark'
  } catch (e) {}
  document.documentElement.setAttribute('data-theme', t);
  if (t === 'light') document.getElementById('thT').checked = true
})();

function tgM() {
  document.getElementById('mnu').classList.toggle('open');
  document.getElementById('mov').classList.toggle('open')
}

function gp(id) {
  document.querySelectorAll('.pg').forEach(p => p.classList.remove('act'));
  document.getElementById(id).classList.add('act');
  document.querySelectorAll('.mnu button:not(.mc)').forEach(b => b.classList.remove('act'));
  const m = {
    dash: 0,
    mon: 1,
    cfg: 2,
    ota: 3
  };
  const btns = document.querySelectorAll('.mnu button:not(.mc)');
  if (m[id] !== undefined) btns[m[id]].classList.add('act');
  tgM();
  if (id === 'cfg') ldCfg()
}

function toast(m, t) {
  const e = document.getElementById('toast');
  e.textContent = m;
  e.className = 'toast show ' + (t || 'ok');
  setTimeout(() => e.className = 'toast', 3000)
}

/* Clock */
setInterval(() => {
  const n = new Date();
  document.getElementById('clk').textContent = String(n.getHours()).padStart(2, '0') + ':' + String(n.getMinutes()).padStart(2, '0') + ':' + String(n.getSeconds()).padStart(2, '0')
}, 1000);

/* Canvas Charts */
const cvs = document.getElementById('cv'),
  cx = cvs.getContext('2d');
let cM = 'v',
  cD = {
    v: [],
    i: []
  };

function sC(m, b) {
  cM = m;
  document.querySelectorAll('.ch-t').forEach(t => t.classList.remove('act'));
  b.classList.add('act');
  dC()
}

function dC() {
  const p = cvs.parentElement;
  if (!p) return;
  const w = p.clientWidth,
    h = p.clientHeight;
  if (w < 2 || h < 2) return;
  cvs.width = w;
  cvs.height = h;
  const d = cD[cM],
    cs = getComputedStyle(document.documentElement);

  if (!d || d.length < 2) {
    cx.fillStyle = cs.getPropertyValue('--bg2').trim();
    cx.fillRect(0, 0, w, h);
    cx.fillStyle = cs.getPropertyValue('--dm').trim();
    cx.font = '10px sans-serif';
    cx.textAlign = 'center';
    cx.fillText('Attesa dati...', w / 2, h / 2);
    return
  }

  const vs = d.map(x => x.v);
  let mn = Math.min(...vs),
    mx = Math.max(...vs);
  const pd = (mx - mn) * .1 || .5;
  mn -= pd;
  mx += pd;

  cx.fillStyle = cs.getPropertyValue('--bg2').trim();
  cx.fillRect(0, 0, w, h);
  cx.strokeStyle = cs.getPropertyValue('--bg3').trim();
  cx.lineWidth = 1;

  for (let y = 0; y <= 3; y++) {
    const py = h - (y / 3) * h;
    cx.beginPath();
    cx.moveTo(0, py);
    cx.lineTo(w, py);
    cx.stroke()
  }

  const clr = cM === 'v' ? cs.getPropertyValue('--ac').trim() : cs.getPropertyValue('--gn').trim();
  cx.strokeStyle = clr;
  cx.lineWidth = 1.5;
  cx.beginPath();
  d.forEach((pt, i) => {
    const x = (i / (d.length - 1)) * w,
      y = h - ((pt.v - mn) / (mx - mn)) * h;
    i === 0 ? cx.moveTo(x, y) : cx.lineTo(x, y)
  });
  cx.stroke();
  cx.lineTo(w, h);
  cx.lineTo(0, h);
  cx.closePath();
  cx.globalAlpha = .05;
  cx.fillStyle = clr;
  cx.fill();
  cx.globalAlpha = 1
}

/* Gauges */
function setGauge(level) {
  var pct = level / 3;
  var off = 251 * (1 - pct);
  document.getElementById('gaugeArc').style.strokeDashoffset = off;
  var angle = -90 + pct * 180;
  document.getElementById('gaugeNeedle').setAttribute('transform', 'rotate(' + angle + ',100,100)');
  document.getElementById('gVal').textContent = level + '/3';
  document.getElementById('gaugeArc').style.stroke = level <= 1 ? 'var(--yw)' : 'var(--ac2)'
}

function fU(s) {
  return Math.floor(s / 86400) + 'd ' + Math.floor(s % 86400 / 3600) + 'h ' + Math.floor(s % 3600 / 60) + 'm'
}

/* Data Fetching */
async function fD() {
  try {
    const r = await fetch('/api/data'),
      d = await r.json();
    document.getElementById('dot').className = 'dot on';
    document.getElementById('simB').style.display = d.sim ? 'inline' : 'none';
    document.getElementById('aO').classList.toggle('on', !!d.alm_ovr);
    document.getElementById('aL').classList.toggle('on', !!d.alm_low);
    document.getElementById('aC').classList.toggle('on', !!d.alm_cur);

    const v = d.v || 0,
      i = d.i || 0,
      w = d.w || 0,
      soc = d.soc || 0,
      cap = d.cap || 100,
      ah = d.ah || 0;
    document.getElementById('vV').textContent = v.toFixed(2);
    document.getElementById('vBv').style.color = d.alm_ovr ? 'var(--rd)' : d.alm_low ? 'var(--yw)' : 'var(--ac)';
    document.getElementById('iV').textContent = i.toFixed(1);
    document.getElementById('iV').style.color = i > 0 ? 'var(--gn)' : i < 0 ? 'var(--yw)' : 'var(--tx)';
    document.getElementById('wV').textContent = Math.abs(w).toFixed(0);

    const sm = {
      Charge: {
        tx: 'Carica',
        cl: 'var(--gn)'
      },
      Discharge: {
        tx: 'Scarica',
        cl: 'var(--yw)'
      },
      Idle: {
        tx: 'Idle',
        cl: 'var(--dm)'
      }
    };
    const s = sm[d.state] || {
      tx: d.state,
      cl: 'var(--dm)'
    };
    document.getElementById('stT').textContent = s.tx;
    document.getElementById('stT').style.color = s.cl;
    document.getElementById('etaV').textContent = d.eta || '--:--';

    const circ = 2 * Math.PI * 50,
      arc = document.getElementById('socA');
    document.getElementById('sP').textContent = Math.round(soc) + '%';
    arc.style.strokeDashoffset = circ * (1 - soc / 100);
    arc.style.stroke = soc > 50 ? 'var(--gn)' : soc > 20 ? 'var(--yw)' : 'var(--rd)';
    document.getElementById('sP').style.color = arc.style.stroke;

    document.getElementById('ahU').textContent = ah.toFixed(1) + ' Ah';
    if (document.getElementById('socVV'))
      document.getElementById('socVV').textContent = (d.socV || 0).toFixed(1) + '%';
    if (document.getElementById('socAhV'))
      document.getElementById('socAhV').textContent = (d.socAh || 0).toFixed(1) + '%';
    document.getElementById('ahC').textContent = cap.toFixed(0) + ' Ah';
    document.getElementById('ahL').textContent = Math.max(0, cap - ah).toFixed(1) + ' Ah';
    document.getElementById('vMn').textContent = (d.vmin || 0).toFixed(2) + ' V';
    document.getElementById('vMx').textContent = (d.vmax || 0).toFixed(2) + ' V';
    document.getElementById('iMn').textContent = (d.imin || 0).toFixed(1) + ' A';
    document.getElementById('iMx').textContent = (d.imax || 0).toFixed(1) + ' A';

    setGauge(d.tank_gray || 0);
    const tb = d.tank_black || 0,
      ti = document.getElementById('tbI'),
      tt = document.getElementById('tbT');
    if (tb) {
      ti.className = 'tb-i fu';
      ti.textContent = '!';
      tt.textContent = 'Svuotare!';
      tt.style.color = 'var(--rd)'
    } else {
      ti.className = 'tb-i';
      ti.textContent = 'OK';
      tt.textContent = 'Libero';
      tt.style.color = 'var(--tx)'
    }

    document.getElementById('d-rs').textContent = d.rssi + ' dBm';
    document.getElementById('d-up').textContent = fU(d.uptime);
    document.getElementById('d-hp').textContent = (d.heap / 1024).toFixed(0) + ' KB';
    document.getElementById('d-ina').textContent = d.connected ? 'OK' : 'SIM';
    document.getElementById('d-ina').style.color = d.connected ? 'var(--gn)' : 'var(--yw)';
    document.getElementById('d-apip').textContent = d.ap_ip || '--';
    const stip = document.getElementById('d-stip');
    stip.textContent = d.sta ? d.sta_ip : 'Non connesso';
    stip.style.color = d.sta ? 'var(--gn)' : 'var(--dm)';

    // Monitor elements
    document.getElementById('m-v').textContent = v.toFixed(2) + ' V';
    document.getElementById('m-i').textContent = i.toFixed(1) + ' A';
    document.getElementById('m-w').textContent = w.toFixed(0) + ' W';
    document.getElementById('m-st').textContent = d.state;
    document.getElementById('m-eta').textContent = d.eta || '--';
    document.getElementById('m-bar').style.width = soc.toFixed(0) + '%';
    document.getElementById('m-soc').textContent = soc.toFixed(1) + '%';
    document.getElementById('m-ah').textContent = ah.toFixed(1) + '/' + cap.toFixed(0) + ' Ah';
    document.getElementById('m-aO').style.display = d.alm_ovr ? 'block' : 'none';
    document.getElementById('m-aL').style.display = d.alm_low ? 'block' : 'none';
    document.getElementById('m-vn').textContent = (d.vmin || 0).toFixed(2) + ' V';
    document.getElementById('m-vx').textContent = (d.vmax || 0).toFixed(2) + ' V';
    document.getElementById('m-in').textContent = (d.imin || 0).toFixed(1) + ' A';
    document.getElementById('m-ix').textContent = (d.imax || 0).toFixed(1) + ' A';

    const tgN = ['Riserva', '1/3', '2/3', 'Pieno'];
    document.getElementById('m-tg').textContent = tgN[d.tank_gray] || d.tank_gray + '/3';
    document.getElementById('m-tb').textContent = tb ? 'PIENO' : 'OK';
    document.getElementById('m-tb').style.color = tb ? 'var(--rd)' : 'var(--gn)';
    document.getElementById('m-a0').textContent = d.adc_blk;
    document.getElementById('m-a1').textContent = d.adc_g1;
    document.getElementById('m-a2').textContent = d.adc_g2;
    document.getElementById('m-a3').textContent = d.adc_g3;
    document.getElementById('m-rs').textContent = d.rssi + ' dBm';
    document.getElementById('m-up').textContent = fU(d.uptime);
    document.getElementById('m-hp').textContent = (d.heap / 1024).toFixed(0) + ' KB';
    document.getElementById('m-ina').textContent = d.connected ? 'OK' : 'SIM';
    document.getElementById('m-ina').style.color = d.connected ? 'var(--gn)' : 'var(--yw)';
    document.getElementById('m-wm').textContent = d.wifiMode || '--';
    document.getElementById('m-ip').textContent = d.ip || '--';
    document.getElementById('m-apip').textContent = d.apip || '--'
  } catch (e) {
    document.getElementById('dot').className = 'dot'
  }
}

async function fH() {
  try {
    const r = await fetch('/api/history'),
      h = await r.json();
    cD.v = (h.voltage || []).map(p => ({ v: typeof p === 'object' ? p.v : p }));
    cD.i = (h.current || []).map(p => ({ v: typeof p === 'object' ? p.v : p }));
    dC()
  } catch (e) {}
}

setInterval(fD, 1000);
setInterval(fH, 5000);
setInterval(dC, 3000);
fD();
fH();

/* Actions */
async function rstMM() {
  try {
    await fetch('/api/reset-minmax', {
      method: 'POST'
    });
    toast('Reset!')
  } catch (e) {
    toast('Errore', 'err')
  }
}

async function ldCfg() {
  try {
    const r = await fetch('/api/config'),
      c = await r.json();
    const s = document.getElementById('c-bt');
    s.innerHTML = '';
    (c.batTypes || []).forEach((n, i) => {
      const o = document.createElement('option');
      o.value = i;
      o.textContent = n;
      s.appendChild(o)
    });
    s.value = c.batType;
    // Popola select voltaggio nominale con valori del profilo
    const nvSel = document.getElementById('c-nv');
    nvSel.innerHTML = '';
    const stdVols = [12.0, 24.0, 36.0, 48.0];
    stdVols.forEach(v => {
      const o = document.createElement('option');
      o.value = v; o.textContent = v + ' V';
      nvSel.appendChild(o);
    });
    nvSel.value = c.nomV || (c.batNomV && c.batNomV[c.batType]) || 12.0;
    document.getElementById('c-cap').value = c.capAh;
    document.getElementById('c-sh').value = c.shuntOhm;
    document.getElementById('c-mi').value = c.maxI;
    document.getElementById('c-vh').value = c.vHigh;
    document.getElementById('c-vl').value = c.vLow;
    document.getElementById('c-ih').value = c.iHigh;
    document.getElementById('c-te').checked = c.tankEn;
    document.getElementById('c-tb').value = c.tBlkThr;
    document.getElementById('c-tg').value = c.tGryThr;
    document.getElementById('c-la0').textContent = 'ADC:' + c.adc_blk;
    document.getElementById('c-la1').textContent = 'ADC:' + c.adc_g1;
    document.getElementById('c-la1b').textContent = c.adc_g1;
    document.getElementById('c-la2').textContent = c.adc_g2;
    document.getElementById('c-la3').textContent = c.adc_g3;
    document.getElementById('c-sda').value = c.pSDA;
    document.getElementById('c-scl').value = c.pSCL;
    document.getElementById('c-pb').value = c.pTBlk;
    document.getElementById('c-pg1').value = c.pTG1;
    document.getElementById('c-pg2').value = c.pTG2;
    document.getElementById('c-pg3').value = c.pTG3;
    document.getElementById('c-pe').value = c.pExc;
    document.getElementById('c-wap').checked = c.wifiAP;
    document.getElementById('c-ws').value = c.wSSID;
    document.getElementById('c-wp').value = c.wPass;
    document.getElementById('c-wss').value = c.wStaSSID;
    document.getElementById('c-wsp').value = c.wStaPass
  } catch (e) {
    toast('Errore config', 'err')
  }
}

async function saveCfg() {
  const b = {
    batType: +document.getElementById('c-bt').value,
    capAh: +document.getElementById('c-cap').value,
    shuntOhm: +document.getElementById('c-sh').value,
    nomV: +document.getElementById('c-nv').value,
    maxI: +document.getElementById('c-mi').value,
    vHigh: +document.getElementById('c-vh').value,
    vLow: +document.getElementById('c-vl').value,
    iHigh: +document.getElementById('c-ih').value,
    tankEn: document.getElementById('c-te').checked,
    tBlkThr: +document.getElementById('c-tb').value,
    tGryThr: +document.getElementById('c-tg').value,
    pSDA: +document.getElementById('c-sda').value,
    pSCL: +document.getElementById('c-scl').value,
    pTBlk: +document.getElementById('c-pb').value,
    pTG1: +document.getElementById('c-pg1').value,
    pTG2: +document.getElementById('c-pg2').value,
    pTG3: +document.getElementById('c-pg3').value,
    pExc: +document.getElementById('c-pe').value,
    wifiAP: document.getElementById('c-wap').checked,
    wSSID: document.getElementById('c-ws').value,
    wPass: document.getElementById('c-wp').value,
    wStaSSID: document.getElementById('c-wss').value,
    wStaPass: document.getElementById('c-wsp').value
  };
  try {
    const r = await fetch('/api/config', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(b)
    });
    const j = await r.json();
    if (j.ok) {
      toast('Salvato!');
      if (j.reboot) toast('Riavvio...')
    } else toast('Errore', 'err')
  } catch (e) {
    toast('Errore', 'err')
  }
}

async function reboot() {
  if (!confirm('Riavviare?')) return;
  try {
    await fetch('/api/reboot', {
      method: 'POST'
    });
    toast('Riavvio...')
  } catch (e) {}
}

/* OTA Management */
function otaUp(t) {
  const fid = t === 'firmware' ? 'fw-f' : 'fs-f',
    pid = t === 'firmware' ? 'fw-p' : 'fs-p',
    bid = t === 'firmware' ? 'fw-b' : 'fs-b',
    tid = t === 'firmware' ? 'fw-t' : 'fs-t';
  const f = document.getElementById(fid).files[0];
  if (!f) {
    toast('Seleziona .bin', 'err');
    return
  }
  const x = new XMLHttpRequest(),
    fd = new FormData();
  fd.append('file', f);
  document.getElementById(pid).style.display = 'block';
  x.upload.addEventListener('progress', function(e) {
    if (e.lengthComputable) {
      const p = Math.round(e.loaded / e.total * 100);
      document.getElementById(bid).style.width = p + '%';
      document.getElementById(tid).textContent = p + '%'
    }
  });
  x.addEventListener('load', function() {
    try {
      const j = JSON.parse(x.responseText);
      toast(j.msg || 'OK', j.ok ? 'ok' : 'err')
    } catch (e) {
      toast('OK')
    }
    setTimeout(() => document.getElementById(pid).style.display = 'none', 2000)
  });
  x.addEventListener('error', function() {
    toast('Errore', 'err')
  });
  x.open('POST', '/api/ota/' + t);
  x.send(fd)
}

async function scanWifi() {
  const btn = document.getElementById('btn-scan');
  const list = document.getElementById('wifi-list');
  
  btn.disabled = true;
  btn.textContent = "...";
  
  try {
    const r = await fetch('/api/wifi-scan');
    const nets = await r.json();
    
    list.innerHTML = "";
    list.style.display = "block";
    
    if (nets.length === 0) {
      list.innerHTML = "<div style='padding:5px; font-size:11px'>Nessuna rete trovata</div>";
    } else {
      nets.forEach(n => {
        const div = document.createElement('div');
        div.style = "padding:6px 10px; border-bottom:1px solid var(--brd); cursor:pointer; font-size:11px; display:flex; justify-content:space-between; align-items:center";
        // Al click inserisce il nome nell'input e nasconde la lista
        div.onclick = () => {
          document.getElementById('c-wss').value = n.ssid;
          list.style.display = "none";
        };
        div.innerHTML = `<span>${n.ssid}</span><span style="color:var(--dm)">${n.rssi} dBm</span>`;
        list.appendChild(div);
      });
    }
  } catch (e) {
    toast('Errore scansione', 'err');
  } finally {
    btn.disabled = false;
    btn.textContent = "SCAN";
  }
}