import SharpTalkModule from './sharptalk.js';

const Module = await SharpTalkModule();
const instance = new Module.SharpTalkInterop();
window.sharpTalk = instance;
instance.Initialize();

const activeTab = document.querySelector('.tab-btn.active');
if (activeTab) {
    const mode = activeTab.id.replace('tab-', '');
    instance.SetMode(mode === 'klattsch' || mode === 'ust');
}

window.dispatchEvent(new Event('sharptalk-ready'));
