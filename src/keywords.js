// One canonical token vocabulary. Aliases never rewrite names, comments, or text.
// Norwegian spellings other than hvis/fra/til/funk/eller/stopp are provisional.
export const keywordGroups = {
  lib: ['lib', 'bibliotek'],
  public: ['public', 'offentlig'],
  use: ['use', 'bruk'],
  var: ['var'],
  func: ['func', 'funk'],
  ret: ['ret', 'retur'],
  obj: ['obj', 'objekt'],
  if: ['if', 'hvis'],
  else: ['else'],
  // The parser resolves this token to a branch or logical operator by context.
  else_or: ['eller'],
  while: ['while', 'mens'],
  for: ['for'],
  in: ['in', 'blant'],
  from: ['from', 'fra'],
  to: ['to', 'til'],
  step: ['step', 'steg'],
  repeat: ['repeat', 'gjenta'],
  times: ['times', 'ganger'],
  break: ['break', 'stopp'],
  continue: ['continue', 'fortsett'],
  as: ['as', 'som'],
  of: ['of', 'av'],
  and: ['and', 'og'],
  or: ['or'],
  not: ['not', 'ikke'],
  is: ['is', 'er'],
  true: ['true', 'sann'],
  false: ['false', 'usann'],
  none: ['none', 'ingen'],
  int: ['int', 'heltall'],
  dec: ['dec', 'desimal'],
  text: ['text', 'tekst'],
  bool: ['bool', 'logisk'],
  list: ['list', 'liste'],
};

export const keywords = new Map(
  Object.entries(keywordGroups).flatMap(([canonical, spellings]) =>
    spellings.map(spelling => [spelling, canonical])),
);
