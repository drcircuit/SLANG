const { contextBridge, ipcRenderer } = require('electron');
const api = {};
for (const name of ['init','locale','folder','tree','read','open','save','confirm','modules','check','run','control','close']) api[name] = (...args) => ipcRenderer.invoke(name, ...args);
api.onRunner = callback => { const handler = (_event, value) => callback(value); ipcRenderer.on('runner', handler); return () => ipcRenderer.removeListener('runner', handler); };
api.onClosing = callback => { ipcRenderer.on('closing', () => callback()); };
contextBridge.exposeInMainWorld('slang', api);
