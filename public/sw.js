// Cleanup worker for browsers that registered the old root-scoped service worker
// (when the synth lived at /). Clears caches, unregisters, and reloads clients
// so they pick up the new marketing site at / and the synth at /app.
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', e => {
  e.waitUntil((async () => {
    const keys = await caches.keys();
    await Promise.all(keys.map(k => caches.delete(k)));
    await self.registration.unregister();
    const clients = await self.clients.matchAll({ type: 'window' });
    clients.forEach(c => c.navigate(c.url));
  })());
});
