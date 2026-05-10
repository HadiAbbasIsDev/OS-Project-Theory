// This file drives the Neural Ops dashboard. It wires up the live clock,
// job submission form, and four Chart.js instances covering GPU and CPU
// utilisation over time, job status distribution, queue depth, and runtime
// versus estimated duration. A one-second polling loop fetches fresh data
// from the three backend API endpoints and updates the charts, resource
// meters, stat chips, job table, and log viewer on every tick.

'use strict';

function updateClock() {
  document.getElementById('clock').textContent =
    new Date().toLocaleTimeString('en-GB', { hour12: false });
}
setInterval(updateClock, 1000);
updateClock();

let selectedPriority = 'Medium';
document.querySelectorAll('.prio-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.prio-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    selectedPriority = btn.dataset.prio;
    document.getElementById('f-priority').value = selectedPriority;
  });
});

document.getElementById('job-form').addEventListener('submit', async e => {
  e.preventDefault();
  const btn = document.getElementById('submit-btn');
  const msg = document.getElementById('submit-msg');
  btn.disabled = true;
  msg.className = 'submit-msg';
  msg.textContent = 'Submitting...';

  const payload = {
    name:               document.getElementById('f-name').value.trim(),
    model_type:         document.getElementById('f-model').value,
    gpu_slots:          parseInt(document.getElementById('f-gpu').value, 10),
    cpu_cores:          parseInt(document.getElementById('f-cpu').value, 10),
    priority:           selectedPriority,
    estimated_duration: parseInt(document.getElementById('f-duration').value, 10),
  };

  try {
    const res = await fetch('/api/jobs', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    const data = await res.json();
    if (data.success) {
      msg.className = 'submit-msg ok';
      msg.textContent = 'Job #' + data.job_id + ' queued';
      document.getElementById('f-name').value = '';
    } else {
      throw new Error(data.error || 'Unknown error');
    }
  } catch (err) {
    msg.className = 'submit-msg err';
    msg.textContent = err.message;
  }
  btn.disabled = false;
  setTimeout(() => { msg.textContent = ''; }, 3000);
});

const chartDefaults = {
  responsive: true,
  maintainAspectRatio: false,
  animation: { duration: 300 },
  plugins: { legend: { display: false }, tooltip: { enabled: true } },
};

const gridColor = 'rgba(255,255,255,0.05)';
const tickColor = '#64748b';

const utilCtx = document.getElementById('utilChart').getContext('2d');
const utilChart = new Chart(utilCtx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      {
        label: 'GPU Used',
        data: [],
        borderColor: '#38bdf8',
        backgroundColor: 'rgba(56,189,248,0.08)',
        borderWidth: 2,
        pointRadius: 0,
        fill: true,
        tension: 0.4,
      },
      {
        label: 'CPU Used',
        data: [],
        borderColor: '#a78bfa',
        backgroundColor: 'rgba(167,139,250,0.08)',
        borderWidth: 2,
        pointRadius: 0,
        fill: true,
        tension: 0.4,
      },
    ],
  },
  options: {
    ...chartDefaults,
    scales: {
      x: { display: false },
      y: {
        min: 0,
        grid: { color: gridColor },
        ticks: { color: tickColor, font: { family: 'JetBrains Mono', size: 10 } },
      },
    },
    plugins: {
      legend: {
        display: true,
        labels: { color: '#94a3b8', font: { family: 'Space Grotesk', size: 11 }, boxWidth: 12 },
      },
      tooltip: { enabled: true },
    },
  },
});

const statusCtx = document.getElementById('statusChart').getContext('2d');
const statusChart = new Chart(statusCtx, {
  type: 'doughnut',
  data: {
    labels: ['Pending', 'Running', 'Completed', 'Preempted'],
    datasets: [{
      data: [0, 0, 0, 0],
      backgroundColor: [
        'rgba(251,191,36,0.75)',
        'rgba(56,189,248,0.75)',
        'rgba(52,211,153,0.75)',
        'rgba(251,113,133,0.75)',
      ],
      borderColor: [
        'rgba(251,191,36,1)',
        'rgba(56,189,248,1)',
        'rgba(52,211,153,1)',
        'rgba(251,113,133,1)',
      ],
      borderWidth: 1.5,
      hoverOffset: 6,
    }],
  },
  options: {
    ...chartDefaults,
    cutout: '68%',
    plugins: {
      legend: {
        display: true,
        position: 'right',
        labels: { color: '#94a3b8', font: { family: 'Space Grotesk', size: 10 }, boxWidth: 10, padding: 8 },
      },
      tooltip: { enabled: true },
    },
  },
});

const queueCtx = document.getElementById('queueChart').getContext('2d');
const queueChart = new Chart(queueCtx, {
  type: 'bar',
  data: {
    labels: [],
    datasets: [
      {
        label: 'Pending',
        data: [],
        backgroundColor: 'rgba(251,191,36,0.6)',
        borderColor: 'rgba(251,191,36,1)',
        borderWidth: 1,
        borderRadius: 3,
      },
      {
        label: 'Running',
        data: [],
        backgroundColor: 'rgba(56,189,248,0.6)',
        borderColor: 'rgba(56,189,248,1)',
        borderWidth: 1,
        borderRadius: 3,
      },
    ],
  },
  options: {
    ...chartDefaults,
    scales: {
      x: { display: false, stacked: true },
      y: {
        stacked: true,
        min: 0,
        grid: { color: gridColor },
        ticks: { color: tickColor, stepSize: 1, font: { family: 'JetBrains Mono', size: 10 } },
      },
    },
    plugins: {
      legend: {
        display: true,
        labels: { color: '#94a3b8', font: { family: 'Space Grotesk', size: 11 }, boxWidth: 12 },
      },
      tooltip: { enabled: true },
    },
  },
});

const runtimeCtx = document.getElementById('runtimeChart').getContext('2d');
const runtimeChart = new Chart(runtimeCtx, {
  type: 'bar',
  data: {
    labels: [],
    datasets: [
      {
        label: 'Estimated (s)',
        data: [],
        backgroundColor: 'rgba(100,116,139,0.5)',
        borderColor: 'rgba(100,116,139,0.9)',
        borderWidth: 1,
        borderRadius: 3,
      },
      {
        label: 'Actual (s)',
        data: [],
        backgroundColor: 'rgba(52,211,153,0.65)',
        borderColor: 'rgba(52,211,153,1)',
        borderWidth: 1,
        borderRadius: 3,
      },
    ],
  },
  options: {
    ...chartDefaults,
    scales: {
      x: {
        ticks: { color: tickColor, font: { family: 'JetBrains Mono', size: 9 }, maxRotation: 30 },
        grid: { display: false },
      },
      y: {
        grid: { color: gridColor },
        ticks: { color: tickColor, font: { family: 'JetBrains Mono', size: 10 } },
      },
    },
    plugins: {
      legend: {
        display: true,
        labels: { color: '#94a3b8', font: { family: 'Space Grotesk', size: 11 }, boxWidth: 12 },
      },
      tooltip: { enabled: true },
    },
  },
});

const MAX_HISTORY = 40;
function pushRing(arr, val) {
  arr.push(val);
  if (arr.length > MAX_HISTORY) arr.shift();
}

const utilTime = [], utilGPU = [], utilCPU = [];
const queueTime = [], queuePending = [], queueRunning = [];

async function pollResources() {
  try {
    const res  = await fetch('/api/resources');
    const data = await res.json();

    const gpuUsed = data.total_gpu - data.avail_gpu;
    const cpuUsed = data.total_cpu - data.avail_cpu;
    const semUsed = data.sem_max   - data.sem_avail;

    document.getElementById('gpu-val').textContent = gpuUsed + ' / ' + data.total_gpu;
    document.getElementById('cpu-val').textContent = cpuUsed + ' / ' + data.total_cpu;
    document.getElementById('sem-val').textContent = semUsed + ' / ' + data.sem_max + ' running';

    setBar('gpu-bar', gpuUsed / data.total_gpu);
    setBar('cpu-bar', cpuUsed / data.total_cpu);
    setBar('sem-bar', semUsed / data.sem_max);

    const label = new Date().toLocaleTimeString('en-GB', { hour12: false });
    pushRing(utilTime, label);
    pushRing(utilGPU, gpuUsed);
    pushRing(utilCPU, cpuUsed);

    utilChart.data.labels           = [...utilTime];
    utilChart.data.datasets[0].data = [...utilGPU];
    utilChart.data.datasets[1].data = [...utilCPU];
    utilChart.update('none');

  } catch (_) {}
}

function setBar(id, ratio) {
  document.getElementById(id).style.width = (Math.min(1, ratio) * 100).toFixed(1) + '%';
}

async function pollJobs() {
  try {
    const res   = await fetch('/api/jobs');
    const data  = await res.json();
    const jobs  = data.jobs;
    const stats = data.stats;

    document.getElementById('c-pending').textContent  = stats.pending   + ' Pending';
    document.getElementById('c-running').textContent  = stats.running   + ' Running';
    document.getElementById('c-done').textContent     = stats.completed + ' Done';
    document.getElementById('c-preempt').textContent  = stats.preempted + ' Preempted';

    statusChart.data.datasets[0].data = [
      stats.pending, stats.running, stats.completed, stats.preempted
    ];
    statusChart.update('none');

    const qlabel = new Date().toLocaleTimeString('en-GB', { hour12: false });
    pushRing(queueTime, qlabel);
    pushRing(queuePending, stats.pending);
    pushRing(queueRunning, stats.running);
    queueChart.data.labels           = [...queueTime];
    queueChart.data.datasets[0].data = [...queuePending];
    queueChart.data.datasets[1].data = [...queueRunning];
    queueChart.update('none');

    const done = jobs.filter(j => j.status === 'Completed').slice(-8);
    runtimeChart.data.labels           = done.map(j => '#' + j.id);
    runtimeChart.data.datasets[0].data = done.map(j => j.est_duration);
    runtimeChart.data.datasets[1].data = done.map(j => Math.round(j.run_ms / 1000));
    runtimeChart.update('none');

    renderTable(jobs);

  } catch (_) {}
}

function fmtMs(ms) {
  if (!ms) return '-';
  const s = Math.floor(ms / 1000);
  if (s < 60) return s + 's';
  return Math.floor(s / 60) + 'm ' + (s % 60) + 's';
}

function renderTable(jobs) {
  const tbody = document.getElementById('job-tbody');
  document.getElementById('tbl-count').textContent =
    jobs.length + ' job' + (jobs.length !== 1 ? 's' : '');

  if (jobs.length === 0) {
    tbody.innerHTML = '<tr class="empty-row"><td colspan="11">No jobs yet, submit one above</td></tr>';
    return;
  }

  const order = { Running: 0, Pending: 1, Preempted: 2, Completed: 3 };
  const sorted = [...jobs].sort((a, b) => {
    const od = order[a.status] - order[b.status];
    return od !== 0 ? od : b.id - a.id;
  });

  tbody.innerHTML = sorted.map(j => {
    const badgeCls = 'badge badge-' + j.status.toLowerCase();
    const dot = j.status === 'Running'
      ? '<span class="pulse-dot" style="width:6px;height:6px"></span>'
      : '';
    const prioCls = 'prio-' + j.priority.toLowerCase();
    const boost = j.aging_boost > 0
      ? ' <span title="Aging boost active" style="color:var(--amber);font-size:.65rem">+' + j.aging_boost + '</span>'
      : '';

    return '<tr>'
      + '<td class="mono" style="color:var(--text-muted)">#' + j.id + '</td>'
      + '<td style="max-width:160px;overflow:hidden;text-overflow:ellipsis" title="' + esc(j.name) + '">' + esc(j.name) + '</td>'
      + '<td style="color:var(--text-dim)">' + esc(j.model_type) + '</td>'
      + '<td class="' + prioCls + '">' + j.priority + boost + '</td>'
      + '<td><span class="' + badgeCls + '">' + dot + j.status + '</span></td>'
      + '<td class="mono">' + j.gpu_slots + '</td>'
      + '<td class="mono">' + j.cpu_cores + '</td>'
      + '<td class="mono">' + fmtMs(j.wait_ms) + '</td>'
      + '<td class="mono">' + fmtMs(j.run_ms) + '</td>'
      + '<td class="mono">' + j.est_duration + 's</td>'
      + '<td>'
      +   '<div class="tbl-progress">'
      +     '<div class="tbl-bar"><div class="tbl-fill" style="width:' + j.progress + '%"></div></div>'
      +     '<span class="tbl-pct">' + j.progress + '%</span>'
      +   '</div>'
      + '</td>'
      + '</tr>';
  }).join('');
}

function esc(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

async function pollLogs() {
  try {
    const res     = await fetch('/api/logs');
    const data    = await res.json();
    const logBody = document.getElementById('log-body');

    logBody.innerHTML = data.logs.map(line => {
      const cls = logLineClass(line);
      return '<div class="log-line ' + cls + '">' + esc(line) + '</div>';
    }).join('');

    if (document.getElementById('autoscroll').checked) {
      logBody.scrollTop = logBody.scrollHeight;
    }
  } catch (_) {}
}

function logLineClass(line) {
  if (line.includes('[SUBMIT]'))  return 'log-submit';
  if (line.includes('[START]'))   return 'log-start';
  if (line.includes('[DONE]'))    return 'log-done';
  if (line.includes('[PREEMPT]')) return 'log-preempt';
  if (line.includes('[AGING]'))   return 'log-aging';
  if (line.includes('[WARN]'))    return 'log-warn';
  return 'log-info';
}

async function tick() {
  await Promise.allSettled([pollResources(), pollJobs(), pollLogs()]);
}

tick();
setInterval(tick, 1000);
