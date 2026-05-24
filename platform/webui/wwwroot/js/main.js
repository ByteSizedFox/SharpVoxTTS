import { dotnet } from './_framework/dotnet.js';

const { getAssemblyExports, getConfig } = await dotnet
    .withDiagnosticTracing(false)
    .create();

const exports = await getAssemblyExports(getConfig().mainAssemblyName);
const ST = exports.SharpTalk.WebUi.SharpTalkInterop;

window.sharpTalk = ST;
ST.Initialize();

// Sync whichever tab the user may have clicked during WASM load
const activeTab = document.querySelector('.tab-btn.active');
if (activeTab) {
    const mode = activeTab.id.replace('tab-', '');
    ST.SetMode(mode === 'klattsch' || mode === 'ust');
}

window.dispatchEvent(new Event('sharptalk-ready'));
