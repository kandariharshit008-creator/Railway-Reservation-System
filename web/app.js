const trainRows = document.querySelector('#train-rows');
const reservationRows = document.querySelector('#reservation-rows');
const trainSelect = document.querySelector('#train-select');
const seatInput = document.querySelector('#seats');
const availabilityNote = document.querySelector('#availability-note');
const formMessage = document.querySelector('#form-message');
const toast = document.querySelector('#toast');
let trains = [];
let toastTimer;

const escapeHtml = (value) => String(value).replace(/[&<>"']/g, (character) => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
})[character]);

async function api(url, options = {}) {
  const response = await fetch(url, options);
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || 'The request could not be completed.');
  return result;
}

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('visible');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove('visible'), 3600);
}

function updateTrainSelect() {
  const previous = trainSelect.value;
  trainSelect.innerHTML = trains.map((train) =>
    `<option value="${train.number}">${escapeHtml(train.name)} · ${escapeHtml(train.source)} to ${escapeHtml(train.destination)}</option>`
  ).join('');
  if (trains.some((train) => String(train.number) === previous)) trainSelect.value = previous;
  updateSeatLimit();
}

function updateSeatLimit() {
  const train = trains.find((item) => String(item.number) === trainSelect.value);
  const available = train ? train.available : 0;
  seatInput.max = Math.max(1, available);
  if (Number(seatInput.value) > available && available > 0) seatInput.value = available;
  availabilityNote.textContent = train
    ? `${available} ${available === 1 ? 'seat' : 'seats'} available on ${train.name}.`
    : 'No seats are currently available on this train.';
  seatInput.disabled = available === 0;
  document.querySelector('#booking-form button[type="submit"]').disabled = available === 0;
}

function renderTrains() {
  if (!trains.length) {
    trainRows.innerHTML = '<tr><td colspan="5" class="empty-cell">No trains are scheduled.</td></tr>';
    return;
  }
  trainRows.innerHTML = trains.map((train) => `
    <tr>
      <td><span class="train-name">${escapeHtml(train.name)}</span><span class="train-number">${train.number}</span></td>
      <td class="route-cell">${escapeHtml(train.source)} <span aria-hidden="true">→</span> ${escapeHtml(train.destination)}</td>
      <td class="departure-cell">${escapeHtml(train.departure)}</td>
      <td><span class="seat-count ${train.available <= 1 ? 'low' : ''}">${train.available}</span> <span class="train-number">/ ${train.capacity}</span></td>
      <td><button class="small-book" type="button" data-train="${train.number}" ${train.available === 0 ? 'disabled' : ''}>Select</button></td>
    </tr>`).join('');
  trainRows.querySelectorAll('[data-train]').forEach((button) => {
    button.addEventListener('click', () => {
      trainSelect.value = button.dataset.train;
      updateSeatLimit();
      document.querySelector('#passenger').focus();
    });
  });
}

function renderReservations(reservations) {
  document.querySelector('#reservation-total').textContent = reservations.length;
  document.querySelector('#booking-count').textContent = reservations.length;
  if (!reservations.length) {
    reservationRows.innerHTML = '<tr><td colspan="6" class="empty-cell">No active reservations yet. New bookings will appear here.</td></tr>';
    return;
  }
  reservationRows.innerHTML = reservations.map((reservation) => `
    <tr>
      <td class="pnr">${reservation.pnr}</td>
      <td class="passenger-name">${escapeHtml(reservation.passenger)}</td>
      <td>${escapeHtml(reservation.trainName)}<span class="train-detail">${escapeHtml(reservation.route)} · ${escapeHtml(reservation.departure)}</span></td>
      <td>${reservation.age}</td>
      <td>${reservation.seats}</td>
      <td><button class="cancel-button" type="button" data-pnr="${reservation.pnr}" aria-label="Cancel reservation ${reservation.pnr}">Cancel</button></td>
    </tr>`).join('');
  reservationRows.querySelectorAll('[data-pnr]').forEach((button) => {
    button.addEventListener('click', () => cancelReservation(button.dataset.pnr));
  });
}

async function refreshData() {
  const [nextTrains, reservations] = await Promise.all([
    api('/api/trains'), api('/api/reservations')
  ]);
  trains = nextTrains;
  updateTrainSelect();
  renderTrains();
  renderReservations(reservations);
  document.querySelector('#train-count').textContent = trains.length;
  document.querySelector('#seat-count').textContent = trains.reduce((sum, train) => sum + train.available, 0);
}

async function cancelReservation(pnr) {
  if (!window.confirm(`Cancel reservation ${pnr}?`)) return;
  try {
    await api(`/api/reservations/${encodeURIComponent(pnr)}`, { method: 'DELETE' });
    await refreshData();
    showToast(`Reservation ${pnr} cancelled.`);
  } catch (error) {
    showToast(error.message);
  }
}

document.querySelector('#booking-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const button = form.querySelector('button[type="submit"]');
  const submitLabel = button.querySelector('span:first-child');
  button.disabled = true;
  submitLabel.textContent = 'Saving reservation…';
  formMessage.textContent = '';
  formMessage.classList.remove('error');
  try {
    const body = new URLSearchParams(new FormData(form));
    const result = await api('/api/book', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    form.reset();
    seatInput.value = 1;
    await refreshData();
    formMessage.textContent = `Confirmed · PNR ${result.pnr}`;
    showToast(`Booking confirmed. Your PNR is ${result.pnr}.`);
  } catch (error) {
    formMessage.textContent = error.message;
    formMessage.classList.add('error');
  } finally {
    submitLabel.textContent = 'Confirm reservation';
    updateSeatLimit();
  }
});

trainSelect.addEventListener('change', updateSeatLimit);
document.querySelector('#refresh-button').addEventListener('click', async () => {
  try {
    await refreshData();
    showToast('Train and booking data refreshed.');
  } catch (error) {
    showToast(error.message);
  }
});

document.querySelector('#today-date').textContent = new Intl.DateTimeFormat('en', {
  weekday: 'short', month: 'short', day: 'numeric', year: 'numeric'
}).format(new Date());

refreshData().catch((error) => {
  trainRows.innerHTML = `<tr><td colspan="5" class="empty-cell">${escapeHtml(error.message)} Start the C++ server and refresh this page.</td></tr>`;
  reservationRows.innerHTML = '<tr><td colspan="6" class="empty-cell">Waiting for the local reservation service.</td></tr>';
  document.querySelector('#train-count').textContent = '—';
  document.querySelector('#seat-count').textContent = '—';
  document.querySelector('#booking-count').textContent = '—';
});
