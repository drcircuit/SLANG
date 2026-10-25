// Application chrome only. Never translate a student's source or program output.
const no = {
  'Function in this file.':'Funksjon i denne filen.', 'Object type':'Objekttype', 'Object field':'Egenskap',
  'Inferred type':'Utledet type', 'Loop variable':'Løkkevariabel',
  'A place to make things.':'Et sted å skape.', 'Program controls':'Programknapper',
  'Run':'Kjør', 'Continue':'Fortsett', 'Stop':'Stopp', 'Pause':'Pause', 'Resume':'Fortsett', 'Debug':'Feilsøk', 'Step':'Steg',
  'Run / Continue (F5)':'Kjør / fortsett (F5)', 'Stop (Shift+F5)':'Stopp (Shift+F5)', 'Pause / Resume (F6)':'Pause / fortsett (F6)',
  'Debug from the first statement (Ctrl+F5)':'Feilsøk fra første instruksjon (Ctrl+F5)', 'Step into the next statement (F11)':'Gå til neste instruksjon (F11)',
  'Appearance':'Utseende', 'Mode':'Modus', 'Theme':'Tema', 'Color mode':'Lys eller mørk', 'Color theme':'Fargetema',
  'Light':'Lys', 'Dark':'Mørk', 'Teal':'Turkis', 'Pink':'Rosa', 'Purple':'Lilla', 'Blue':'Blå', 'Green':'Grønn', 'Yellow':'Gul', 'Orange':'Oransje', 'High contrast':'Høy kontrast',
  'Language':'Språk', 'FILES':'FILER', 'Files':'Filer', 'New file':'Ny fil', 'New file (Ctrl+N)':'Ny fil (Ctrl+N)', 'Refresh files':'Oppdater filer',
  'Open folder':'Åpne mappe', 'Open file':'Åpne fil', 'Open file (Ctrl+O)':'Åpne fil (Ctrl+O)',
  'Resize file browser':'Endre bredden på filoversikten', 'Resize console':'Endre høyden på konsollen', 'Resize canvas panel':'Endre bredden på lerretet',
  'Save':'Lagre', 'Save as':'Lagre som', 'Save (Ctrl+S)':'Lagre (Ctrl+S)', 'Save as (Ctrl+Shift+S)':'Lagre som (Ctrl+Shift+S)',
  'SLANG code editor':'SLANG-koderedigering', 'CONSOLE':'KONSOLL', 'Program output':'Programutskrift', 'Clear':'Tøm', 'Program console':'Programkonsoll',
  'Console input':'Svar til programmet', 'Input becomes available when your program asks for it':'Her kan du svare når programmet spør', 'Send ↵':'Send ↵', 'End input':'Avslutt inndata',
  'CANVAS':'LERRET', 'SDL graphics':'SDL-grafikk', 'Close':'Lukk', 'Close the program canvas':'Lukk programmets lerret',
  'Program canvas. Click to use the keyboard.':'Programmets lerret. Klikk for å bruke tastaturet.',
  'Give your ideas a shape.':'Gi ideene dine en form.', 'Draw something or run an animation.':'Tegn noe eller kjør en animasjon.', 'Your canvas will appear here.':'Lerretet ditt vises her.',
  'Try a drawing':'Prøv en tegning', 'Click the canvas to send keyboard input':'Klikk på lerretet for å bruke tastaturet',
  'Keyboard connected · click the editor to return':'Tastaturet er koblet til · klikk i koden for å gå tilbake',
  'VARIABLES':'VARIABLER', 'Debugger':'Feilsøking', 'Click the gutter beside a line to set a breakpoint.':'Klikk i margen ved en linje for å sette et stoppunkt.',
  'Start with Debug to step through your program.':'Start med Feilsøk for å gå gjennom programmet stegvis.', 'A little help':'Litt hjelp',
  'Syntax checking ready':'Syntakskontroll klar', 'Syntax checker unavailable':'Syntakskontroll er utilgjengelig', '✓ Syntax OK':'✓ Syntaks OK',
  'Unsaved changes':'Ulagrede endringer', 'unsaved':'ulagret', 'Ready':'Klar', 'Running':'Kjører', 'Starting':'Starter', 'Stopping':'Stopper', 'Paused':'På pause', 'Waiting for input':'Venter på svar',
  'Debug session':'Feilsøkingsøkt', 'Breakpoint — click to remove':'Stoppunkt - klikk for å fjerne',
  'No variables at this point. Step to the next statement to inspect its scope.':'Ingen variabler her. Gå til neste instruksjon for å se variablene der.',
  'Program finished.':'Programmet er ferdig.', 'Program stopped.':'Programmet er stoppet.', 'Program ended with an error.':'Programmet sluttet med en feil.',
  'success':'ferdig', 'stopped':'stoppet', 'error':'feil', 'pause':'pause', 'step':'steg', 'breakpoint':'stoppunkt',
  'Program window keyboard input':'Tastatur for programvinduet', 'Discard':'Forkast', 'Cancel':'Avbryt', 'SLANG source':'SLANG-kildekode', 'Text files':'Tekstfiler',
  'Invalid file path':'Ugyldig filnavn', 'Choose this file or folder with Open first.':'Velg filen eller mappen med Åpne først.',
  'Open a text file smaller than 2 MB.':'Åpne en tekstfil som er mindre enn 2 MB.', 'This is a binary file. Choose a SLANG source file.':'Dette er en binærfil. Velg en SLANG-kildefil.',
  'Source exceeds 2 MB.':'Kildekoden er større enn 2 MB.', 'Stop the current program first.':'Stopp programmet som kjører først.'
};
function normalizeLocale(value) { return value === 'no' ? 'no' : 'en'; }
function translate(locale, message) { return normalizeLocale(locale) === 'no' ? no[message] || message : message; }
module.exports = { normalizeLocale, translate };
