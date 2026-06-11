/**
 * 记事簿 — C++ Qt WebChannel Bridge Version
 * ============================================
 * Uses QWebChannel instead of Flask fetch().
 * API object is exposed as `api` via QWebChannel.
 */
/* global QWebChannel, api */

let currentTab='notes',currentNoteId=null,alwaysOnTop=false;
let isDirty=false,isDirtyReminder=false,isSaving=false,searchTimer=null;
let _reminderPollTimer=null;

const $=s=>document.querySelector(s),$$=s=>document.querySelectorAll(s);
const notesList=$('#notesList'),notesEmpty=$('#notesEmpty');
const remindersList=$('#remindersList'),remindersEmpty=$('#remindersEmpty');
const toastContainer=$('#toastContainer'),searchInput=$('#searchInput');

// ── Wait for QWebChannel, then init ──
function waitForApi(retries) {
  if (typeof api !== 'undefined' && api && api.getNotes) { init(); return; }
  if (retries > 200) { console.error('WebChannel API not available'); return; }
  setTimeout(function(){ waitForApi((retries||0)+1); }, 50);
}

// ── Helpers ──
function esc(s){if(s==null)return'';var d=document.createElement('div');d.textContent=s;return d.innerHTML;}
function toast(m,d){d=d||2500;var e=document.createElement('div');e.className='toast';e.textContent=m;toastContainer.appendChild(e);setTimeout(function(){e.classList.add('removing');setTimeout(function(){e.remove();},200);},d);}
function bridgeCmd(cmd){ try{api[cmd]();}catch(e){} }

// ── Notes ──
async function loadNotes(){
  try{var notes=JSON.parse(JSON.stringify(api.getNotes()));}catch(e){notes=[];}
  var q=(searchInput.value||'').toLowerCase().trim();
  var f=q?notes.filter(function(n){return n.title.toLowerCase().indexOf(q)>=0||(n.content||'').toLowerCase().indexOf(q)>=0;}):notes;
  renderNoteList(f,q);updateStats(notes,f);
}
function updateStats(all,f){var e=$('#notesStats');if(!e)return;var t=all.length,c=all.filter(function(n){return n.is_completed;}).length,a=t-c;if(!t){e.textContent='';return;}e.textContent=t+' 条笔记 ('+a+' 活跃'+(c>0?', '+c+' 已完成':'')+')'+(f.length!==t?' — 显示 '+f.length+' 条':'');}
function renderNoteList(notes,q){
  notesList.innerHTML='';
  if(!notes.length){notesEmpty.classList.add('visible');notesEmpty.querySelector('.empty-title').textContent=q?'未找到匹配的笔记':'还没有笔记';notesEmpty.querySelector('.empty-desc').textContent=q?'尝试其他关键词':'点击 + 按钮创建第一条笔记';notesList.style.display='none';return;}
  notesEmpty.classList.remove('visible');notesList.style.display='';
  notes.forEach(function(n){notesList.appendChild(createNoteCard(n));});
}
function createNoteCard(note){
  var d=document.createElement('div');
  d.className='note-card glass'+(note.is_completed?' completed':'');
  d.dataset.id=note.id;d.tabIndex=0;d.setAttribute('role','listitem');
  var pl={1:'低',2:'中',3:'高'};
  var pri=note.priority>0?'<span class="badge badge-priority badge-p'+note.priority+'">⚡ '+pl[note.priority]+'</span>':'';
  var cat=note.category?'<span class="badge">'+esc(note.category)+'</span>':'';
  d.innerHTML='<div class="note-card-header"><span class="note-card-title">'+(esc(note.title)||'无标题')+'</span><div class="note-card-check" data-action="toggle" data-id="'+note.id+'">'+(note.is_completed?'<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3"><polyline points="20 6 9 17 4 12"/></svg>':'')+'</div></div>'+(note.content?'<div class="note-card-preview">'+esc(note.content)+'</div>':'')+'<div class="note-card-badges">'+cat+pri+'</div>';
  d.addEventListener('click',function(e){if(e.target.closest('[data-action="toggle"]'))return;openEditor(note.id);});
  d.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();openEditor(note.id);}});
  var check=d.querySelector('[data-action="toggle"]');
  if(check)check.addEventListener('click',function(e){e.stopPropagation();
    try{var u=api.toggleNoteComplete(note.id);if(u){note.is_completed=u.is_completed;d.classList.toggle('completed',u.is_completed);var svg=check.querySelector('svg');if(u.is_completed){if(!svg)check.innerHTML='<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3"><polyline points="20 6 9 17 4 12"/></svg>';}else{if(svg)svg.remove();}}}catch(err){toast('操作失败');} });
  return d;
}

// ── Editor ──
async function openEditor(noteId){
  if(isDirty&&!confirm('有未保存的更改，确定放弃吗？'))return;
  currentNoteId=noteId;isDirty=false;
  var overlay=$('#editorOverlay');
  if(noteId===null){
    $('#editorTitle').textContent='新建笔记';
    $('#editorInputTitle').value=$('#editorTextarea').value='';
    $('#editorCategory').value='';$('#editorPriority').value='0';
    $('#editorHint').textContent='';
    $('#btnEditorDelete').style.display='none';$('#btnEditorPin').style.display='none';
  }else{
    $('#editorTitle').textContent='编辑笔记';
    try{var note=api.getNote(noteId);if(!note||!note.id){toast('笔记不存在');return;}
      $('#editorInputTitle').value=note.title||'';$('#editorTextarea').value=note.content||'';
      $('#editorCategory').value=note.category||'';$('#editorPriority').value=String(note.priority||0);
      $('#editorHint').textContent='创建于 '+note.created_at;
      $('#btnEditorDelete').style.display='';$('#btnEditorPin').style.display='';
      $('#btnEditorPin').classList.toggle('pinned',!!note.is_pinned);
    }catch(err){toast('加载失败');return;}
  }
  overlay.classList.add('visible');
  setTimeout(function(){$('#editorInputTitle').focus();},250);
  var track=function(){isDirty=true;};
  $('#editorInputTitle').addEventListener('input',track,{once:true});
  $('#editorTextarea').addEventListener('input',track,{once:true});
}
function closeEditor(silent){
  if(!silent&&isDirty){if(!confirm('有未保存的更改，确定放弃吗？'))return;}
  $('#editorOverlay').classList.remove('visible');currentNoteId=null;isDirty=false;
}
function saveNote(){
  if(isSaving)return;
  var title=$('#editorInputTitle').value.trim();
  var content=$('#editorTextarea').value.trim();
  var category=$('#editorCategory').value;
  var priority=parseInt($('#editorPriority').value)||0;
  if(!title){toast('请输入标题');return;}
  isSaving=true;var btn=$('#btnEditorSave');btn.textContent='保存中...';btn.disabled=true;
  try{
    if(currentNoteId===null){api.addNote(title,content,priority,category);toast('笔记已创建 ✨');}
    else{api.updateNote(currentNoteId,{title:title,content:content,category:category,priority:priority});toast('笔记已更新 ✓');}
    isDirty=false;closeEditor(true);loadNotes();
  }catch(err){toast('保存失败');}
  finally{isSaving=false;btn.textContent='保存';btn.disabled=false;}
}
function deleteCurrentNote(){
  if(currentNoteId===null)return;if(!confirm('确定要删除这条笔记吗？'))return;
  try{api.deleteNote(currentNoteId);toast('笔记已删除');isDirty=false;closeEditor(true);loadNotes();}
  catch(err){toast('删除失败');}
}
function pinCurrentNote(){
  if(currentNoteId===null)return;
  try{var r=api.toggleNotePin(currentNoteId);if(r){$('#btnEditorPin').classList.toggle('pinned',r.is_pinned);toast(r.is_pinned?'已置顶 📌':'已取消置顶');loadNotes();}}
  catch(err){toast('操作失败');}
}

// ── Reminders ──
function loadReminders(){try{renderReminderList(JSON.parse(JSON.stringify(api.getReminders())));}catch(e){renderReminderList([]);}}
function renderReminderList(reminders){
  remindersList.innerHTML='';
  if(!reminders.length){remindersEmpty.classList.add('visible');remindersList.style.display='none';return;}
  remindersEmpty.classList.remove('visible');remindersList.style.display='';
  reminders.forEach(function(r){remindersList.appendChild(createReminderCard(r));});
}
function createReminderCard(reminder){
  var d=document.createElement('div');
  d.className='reminder-card glass'+(reminder.is_enabled?'':' disabled');
  var dd='',tt='';
  try{var dt=new Date(reminder.trigger_at.replace(' ','T'));if(!isNaN(dt)){dd=dt.getFullYear()+'/'+(dt.getMonth()+1)+'/'+dt.getDate();tt=String(dt.getHours()).padStart(2,'0')+':'+String(dt.getMinutes()).padStart(2,'0');}}
  catch(e){dd='--';tt=reminder.trigger_at;}
  var rl={5:'每5分钟',15:'每15分钟',30:'每30分钟',60:'每小时',1440:'每天',10080:'每周'};
  var ri=reminder.is_repeated&&reminder.repeat_interval_min?'🔄 '+(rl[reminder.repeat_interval_min]||'每'+reminder.repeat_interval_min+'分钟'):'';
  var dl=!reminder.is_enabled?' <span class="badge" style="opacity:0.6">已停用</span>':'';
  d.innerHTML='<div class="reminder-time-badge"><span class="reminder-time-date">'+dd+'</span><span class="reminder-time-time">'+tt+'</span></div><div class="reminder-info"><div class="reminder-info-title">'+esc(reminder.title)+dl+'</div>'+(reminder.description?'<div class="reminder-info-desc">'+esc(reminder.description)+'</div>':'')+(ri?'<div class="reminder-info-repeat">'+ri+'</div>':'')+'</div><div class="reminder-actions"><button class="btn-glass btn-icon btn-sm" data-action="toggle-reminder" data-id="'+reminder.id+'" title="'+(reminder.is_enabled?'停用':'启用')+'"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg></button><button class="btn-glass btn-icon btn-sm" data-action="edit-reminder" data-id="'+reminder.id+'" title="编辑"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg></button><button class="btn-glass btn-icon btn-sm" data-action="delete-reminder" data-id="'+reminder.id+'" title="删除"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg></button></div>';
  d.querySelector('[data-action="toggle-reminder"]').addEventListener('click',function(e){e.stopPropagation();
    try{var u=api.updateReminder(reminder.id,{is_enabled:reminder.is_enabled?0:1});if(u){reminder.is_enabled=u.is_enabled;loadReminders();}}catch(err){toast('操作失败');}});
  d.querySelector('[data-action="edit-reminder"]').addEventListener('click',function(e){e.stopPropagation();openReminderForm(reminder);});
  d.querySelector('[data-action="delete-reminder"]').addEventListener('click',function(e){e.stopPropagation();if(!confirm('确定要删除这个提醒吗？'))return;try{api.deleteReminder(reminder.id);toast('提醒已删除');loadReminders();}catch(err){toast('删除失败');}});
  return d;
}
var editingReminderId=null;
function openReminderForm(existing){
  if(isDirtyReminder&&!confirm('有未保存的提醒，确定放弃吗？'))return;isDirtyReminder=false;
  if(existing){editingReminderId=existing.id;
    var dt=new Date(existing.trigger_at.replace(' ','T'));
    if(!isNaN(dt)){$('#reminderDate').value=dt.toISOString().slice(0,10);$('#reminderTime').value=String(dt.getHours()).padStart(2,'0')+':'+String(dt.getMinutes()).padStart(2,'0');}
    $('#reminderInputTitle').value=existing.title||'';$('#reminderTextarea').value=existing.description||'';$('#reminderRepeat').value=String(existing.repeat_interval_min||0);
  }else{editingReminderId=null;
    var nm=new Date(Date.now()+60000);$('#reminderDate').value=nm.toISOString().slice(0,10);$('#reminderTime').value=String(nm.getHours()).padStart(2,'0')+':'+String(nm.getMinutes()).padStart(2,'0');
    $('#reminderInputTitle').value=$('#reminderTextarea').value='';$('#reminderRepeat').value='0';}
  $('#reminderOverlay').classList.add('visible');setTimeout(function(){$('#reminderInputTitle').focus();},300);
  var track=function(){isDirtyReminder=true;};$('#reminderInputTitle').addEventListener('input',track,{once:true});$('#reminderTextarea').addEventListener('input',track,{once:true});
}
function closeReminderForm(silent){if(!silent&&isDirtyReminder){if(!confirm('有未保存的提醒，确定放弃吗？'))return;}$('#reminderOverlay').classList.remove('visible');editingReminderId=null;isDirtyReminder=false;}
function saveReminder(){
  if(isSaving)return;var title=$('#reminderInputTitle').value.trim();if(!title){toast('请输入提醒标题');return;}
  var date=$('#reminderDate').value,time=$('#reminderTime').value;if(!date||!time){toast('请选择提醒日期和时间');return;}
  var triggerAt=date+' '+time+':00';var td=new Date(triggerAt.replace(' ','T'));
  if(td<new Date()&&!editingReminderId){if(!confirm('提醒时间已过期，确定要添加吗？（将立即触发）'))return;}
  var desc=$('#reminderTextarea').value.trim();var rp=parseInt($('#reminderRepeat').value)||0;
  isSaving=true;var btn=$('#btnReminderSave');btn.textContent='保存中...';btn.disabled=true;
  try{
    var data={title:title,trigger_at:triggerAt,description:desc,is_repeated:rp>0,repeat_interval_min:rp};
    if(editingReminderId){api.updateReminder(editingReminderId,data);toast('提醒已更新 ✓');}
    else{api.addReminder(data);toast('提醒已添加 ⏰');}
    isDirtyReminder=false;closeReminderForm(true);loadReminders();
  }catch(err){toast('保存失败');}
  finally{isSaving=false;btn.textContent='保存';btn.disabled=false;}
}

// ── Tab Nav ──
function switchTab(tab){if(tab===currentTab)return;if(isDirty&&!confirm('有未保存的更改，确定放弃吗？'))return;if(isDirtyReminder&&!confirm('有未保存的提醒，确定放弃吗？'))return;currentTab=tab;$$('.tabnav-item').forEach(function(b){b.classList.toggle('active',b.dataset.tab===tab);});$$('.view').forEach(function(v){v.classList.toggle('active',v.id==='view-'+tab);});closeEditor(true);closeReminderForm(true);if(tab==='notes')loadNotes();if(tab==='reminders')loadReminders();}

// ── Settings ──
function loadSettings(){
  try{
    alwaysOnTop=api.getSetting('always_on_top')==='true';
    $('#toggleOnTop').checked=alwaysOnTop;
    $('#toggleAutoStart').checked=api.getSetting('auto_start')==='true';
    $('#toggleCloseToTray').checked=api.getSetting('close_to_tray')==='true';
    $('#btnPin').classList.toggle('pinned',alwaysOnTop);
    var theme=api.getSetting('theme')||'default';applyTheme(theme);
    var ts=$('#themeSelect');if(ts)ts.value=theme;
  }catch(e){}
}
function applyTheme(name){document.documentElement.setAttribute('data-theme',name);try{api.setSetting('theme',name);}catch(e){}try{api.updateThemeBg?api.updateThemeBg(name):null;}catch(e){}}

// ── Event Bindings ──
$$('.tabnav-item').forEach(function(b){b.addEventListener('click',function(){switchTab(b.dataset.tab);});});
searchInput.addEventListener('input',function(){clearTimeout(searchTimer);searchTimer=setTimeout(function(){if(currentTab==='notes')loadNotes();updateClearBtn();},250);});
$('#searchClear')?.addEventListener('click',function(){searchInput.value='';loadNotes();updateClearBtn();});
function updateClearBtn(){var b=$('#searchClear');if(b)b.style.display=searchInput.value?'':'none';}
$('#btnNewNote').addEventListener('click',function(){openEditor(null);});
$('#btnEditorBack').addEventListener('click',function(){closeEditor();});
$('#btnEditorSave').addEventListener('click',saveNote);
$('#btnEditorDelete').addEventListener('click',deleteCurrentNote);
$('#btnEditorPin').addEventListener('click',pinCurrentNote);
$('#editorOverlay').addEventListener('click',function(e){if(e.target===e.currentTarget)closeEditor();});
$('#btnNewReminder').addEventListener('click',function(){openReminderForm(null);});
$('#btnReminderBack').addEventListener('click',function(){closeReminderForm();});
$('#btnReminderSave').addEventListener('click',saveReminder);
$('#reminderOverlay').addEventListener('click',function(e){if(e.target===e.currentTarget)closeReminderForm();});

// Titlebar buttons — direct C++ bridge calls
$('#btnMin')?.addEventListener('click',function(){try{api.minimizeWindow();}catch(e){}});
$('#btnClose')?.addEventListener('click',function(){try{api.closeWindow();}catch(e){}});
$('#btnPin')?.addEventListener('click',function(){
  try{var now=api.getSetting('always_on_top')==='true';api.toggleOnTop();$('#btnPin').classList.toggle('pinned',!now);}catch(e){}
});

// Settings toggles
$('#toggleOnTop')?.addEventListener('change',function(){
  alwaysOnTop=this.checked;
  try{api.setSetting('always_on_top',this.checked?'true':'false');api.setAlwaysOnTop(this.checked);}catch(e){}
  $('#btnPin').classList.toggle('pinned',this.checked);
  toast(this.checked?'窗口置顶已开启':'窗口置顶已关闭');
});
$('#toggleAutoStart')?.addEventListener('change',function(){
  try{var ok=api.setAutoStart(this.checked);toast(ok?(this.checked?'已开启开机自启':'已关闭开机自启'):'操作失败，请检查权限');if(!ok)this.checked=!this.checked;}catch(e){this.checked=!this.checked;}
});
$('#toggleCloseToTray')?.addEventListener('change',function(){
  try{api.setSetting('close_to_tray',this.checked?'true':'false');toast(this.checked?'关闭时将最小化到托盘':'关闭时将退出程序');}catch(e){}
});
$('#themeSelect')?.addEventListener('change',function(){applyTheme(this.value);toast('风格已切换');});

// Keyboard
document.addEventListener('keydown',function(e){
  var ctrl=e.ctrlKey||e.metaKey;
  var inInput=['INPUT','TEXTAREA','SELECT'].includes(document.activeElement?.tagName);
  if(ctrl&&e.key==='f'){e.preventDefault();if(currentTab!=='notes')switchTab('notes');searchInput.focus();searchInput.select();}
  else if(ctrl&&e.key==='n'){e.preventDefault();if(currentTab!=='notes')switchTab('notes');openEditor(null);}
  else if(ctrl&&e.key==='s'){e.preventDefault();if($('#editorOverlay').classList.contains('visible'))saveNote();}
  else if(ctrl&&e.key==='w'){e.preventDefault();closeEditor();closeReminderForm();}
  else if(ctrl&&e.key==='q'){e.preventDefault();try{api.closeWindow();}catch(e){}}
  else if(ctrl&&e.key==='1'){e.preventDefault();switchTab('notes');}
  else if(ctrl&&e.key==='2'){e.preventDefault();switchTab('reminders');}
  else if(ctrl&&e.key==='3'){e.preventDefault();switchTab('settings');}
  else if(e.key==='Escape'){var a=document.activeElement;if(a&&a.tagName==='SELECT')return;if($('#editorOverlay').classList.contains('visible'))closeEditor();else if($('#reminderOverlay').classList.contains('visible'))closeReminderForm();}
});

// Reminder triggered callback
window.onReminderTriggered = function(reminder) {
  toast('⏰ '+reminder.title, 6000);
  if(currentTab==='reminders') loadReminders();
};

// ── Init ──
function init(){
  loadSettings();
  loadNotes();
  loadReminders();
}
if(typeof QWebChannel!=='undefined') {
  new QWebChannel(qt.webChannelTransport,function(channel){window.api=channel.objects.api;waitForApi(0);});
} else {
  waitForApi(0);
}
