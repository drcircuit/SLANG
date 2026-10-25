export const palettes = {
  teal: { label: 'Teal', light: '#087770', dark: '#79d1c7' },
  pink: { label: 'Pink', light: '#a62b69', dark: '#f59ac5' },
  purple: { label: 'Purple', light: '#6d41b1', dark: '#bf9cf4' },
  blue: { label: 'Blue', light: '#1d62b8', dark: '#8bb9ff' },
  green: { label: 'Green', light: '#287343', dark: '#80cca0' },
  yellow: { label: 'Yellow', light: '#806000', dark: '#f5d76e' },
  orange: { label: 'Orange', light: '#a34d0e', dark: '#f8b16c' },
  contrast: { label: 'High contrast', light: '#000080', dark: '#ffff00' },
};
export function normalizeTheme(value, darkDefault = false) {
  return { mode: ['light','dark'].includes(value?.mode) ? value.mode : darkDefault ? 'dark' : 'light', palette: Object.hasOwn(palettes, value?.palette) ? value.palette : 'teal' };
}
const mix = (a, b, fraction) => '#' + [1,3,5].map(i => Math.round(parseInt(a.slice(i,i+2),16) * (1-fraction) + parseInt(b.slice(i,i+2),16) * fraction).toString(16).padStart(2,'0')).join('');
export function createTheme(preference) {
  const { mode, palette } = normalizeTheme(preference), dark = mode === 'dark', highContrast = palette === 'contrast';
  const accent = palettes[palette][mode], surface = dark ? highContrast ? '#000000' : '#191e27' : '#ffffff';
  const text = dark ? highContrast ? '#ffffff' : '#e4eaf2' : highContrast ? '#000000' : '#273649';
  const muted = highContrast ? text : dark ? '#acbacb' : '#596a7d';
  const tokens = {
    surface, text, muted, accent,
    background: highContrast ? surface : mix(dark ? '#121720' : '#f3f5f8', accent, .035),
    panel: highContrast ? surface : mix(dark ? '#1d242f' : '#f7f9fb', accent, .025),
    border: highContrast ? text : dark ? '#465364' : '#c6ced8',
    hover: highContrast ? (dark ? '#222222' : '#e5e5e5') : mix(surface, accent, dark ? .13 : .08),
    selected: highContrast ? (dark ? '#282800' : '#e5e5ff') : mix(surface, accent, dark ? .19 : .11),
    'on-accent': dark ? '#10151c' : '#ffffff',
    'accent-hover': mix(accent, dark ? '#ffffff' : '#000000', .12),
    error: dark ? '#ff9ca7' : '#b02c40',
    success: dark ? '#86d9ad' : '#286e47',
    warning: dark ? '#ffd66b' : '#805b00',
    'debug-line': dark ? '#42391b' : '#fff1bd',
    'toast-bg': text, 'toast-text': surface,
  };
  const syntax = {
    keyword: accent, identifier: text, delimiter: text,
    string: highContrast ? dark ? '#00ffff' : '#005500' : dark ? '#ebc28d' : '#835719',
    number: highContrast ? dark ? '#ffbfff' : '#700070' : dark ? '#d7a5e6' : '#854b91',
    comment: muted,
    predefined: highContrast ? dark ? '#aaffaa' : '#000080' : dark ? '#92c7f4' : '#2c6897',
  };
  return {
    mode, palette, highContrast, tokens,
    monaco: {
      base: highContrast ? dark ? 'hc-black' : 'hc-light' : dark ? 'vs-dark' : 'vs', inherit: true,
      rules: Object.entries(syntax).map(([token, color]) => ({ token, foreground: color.slice(1), ...(token === 'keyword' ? { fontStyle: 'bold' } : token === 'comment' ? { fontStyle: 'italic' } : {}) })),
      colors: {
        'editor.background': surface, 'editor.foreground': text, 'editorLineNumber.foreground': muted,
        'editorLineNumber.activeForeground': accent, 'editor.lineHighlightBackground': tokens.hover,
        'editor.selectionBackground': tokens.selected, 'editor.inactiveSelectionBackground': tokens.hover,
        'editor.selectionHighlightBackground': tokens.selected, 'editorCursor.foreground': accent,
        'editorIndentGuide.background1': tokens.border, 'editorIndentGuide.activeBackground1': muted,
        'editorGutter.background': surface, 'editorError.foreground': tokens.error,
        'editorWarning.foreground': tokens.warning, 'focusBorder': accent, 'foreground': text,
        'editorWidget.background': tokens.panel, 'editorWidget.foreground': text, 'editorWidget.border': tokens.border,
        'editorSuggestWidget.background': tokens.panel, 'editorSuggestWidget.foreground': text,
        'editorSuggestWidget.border': tokens.border, 'editorSuggestWidget.selectedBackground': tokens.selected,
        'editorSuggestWidget.selectedForeground': text, 'editorSuggestWidget.highlightForeground': accent,
        'editorSuggestWidget.focusHighlightForeground': accent, 'editorHoverWidget.background': tokens.panel,
        'editorHoverWidget.foreground': text, 'editorHoverWidget.border': tokens.border,
        'input.background': surface, 'input.foreground': text, 'input.border': tokens.border,
        'list.activeSelectionBackground': tokens.selected, 'list.activeSelectionForeground': text,
        'list.hoverBackground': tokens.hover, 'scrollbarSlider.background': `${muted}60`,
        'scrollbarSlider.hoverBackground': `${muted}90`,
        ...(highContrast ? { 'contrastBorder': text, 'contrastActiveBorder': accent, 'editor.selectionForeground': text } : {}),
      },
    },
  };
}
export function installThemes(monaco, root = document.documentElement, storage = localStorage) {
  let saved;
  try { saved = JSON.parse(storage.getItem('slang.appearance')); } catch { /* A damaged preference must not prevent opening the editor. */ }
  let current = normalizeTheme(saved, matchMedia('(prefers-color-scheme: dark)').matches);
  function apply(value, persist = true) {
    current = normalizeTheme(value);
    const theme = createTheme(current);
    for (const [name,color] of Object.entries(theme.tokens)) root.style.setProperty(`--${name}`, color);
    root.dataset.mode = theme.mode; root.dataset.palette = theme.palette; root.dataset.contrast = String(theme.highContrast);
    root.style.colorScheme = theme.mode;
    monaco.editor.defineTheme('slang-appearance', theme.monaco); monaco.editor.setTheme('slang-appearance');
    if (persist) { try { storage.setItem('slang.appearance', JSON.stringify(current)); } catch { /* Theme still works for this session if storage is unavailable. */ } }
    return current;
  }
  apply(current, false);
  return { current: () => current, apply };
}
