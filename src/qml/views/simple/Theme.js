.pragma library

// Colours from the Simple mode mockup (docs/bugs/design/Main.dc.html).
var dark = {
    bg: '#111214', bar: '#141518', panel: '#17181C', border: '#24272D',
    text: '#ECEDEF', text2: '#C4C7CE', muted: '#9A9EA8', icon: '#A6AAB3',
    accent: '#5B8CFF', onAccent: '#0B1020', accentSoft: '#1D2230',
    chip: '#1D1F24', softBorder: '#2C2F36', dashed: '#3A3E46', drop: '#1B1D22',
    strong: '#ECEDEF', onStrong: '#111214', seg: '#111214', segActive: '#2A2D34',
    countBg: '#2C2F36', trim: '#F5C542', onTrim: '#3A2C00', danger: '#E5484D',
    done: '#2E9C74', red: '#E5392B', mark: 'bugs-mark-on-dark.png'
};

var light = {
    bg: '#F6F7F9', bar: '#FFFFFF', panel: '#FFFFFF', border: '#E3E5EA',
    text: '#16181D', text2: '#3F444E', muted: '#5F6470', icon: '#5F6470',
    accent: '#2F62E0', onAccent: '#FFFFFF', accentSoft: '#EAF0FD',
    chip: '#F0F2F5', softBorder: '#D5D8DE', dashed: '#C9CDD5', drop: '#F6F7F9',
    strong: '#16181D', onStrong: '#FFFFFF', seg: '#EEF0F3', segActive: '#FFFFFF',
    countBg: '#E3E5EA', trim: '#F2B705', onTrim: '#3A2C00', danger: '#E5484D',
    done: '#2E9C74', red: '#E5392B', mark: 'bugs-mark-on-light.png'
};

function colors(isDark) {
    return isDark ? dark : light;
}

// Icon outlines from the mockup, on a 24 by 24 grid.
var icons = {
    shield: 'M12 3l8 3v6c0 4.5-3.4 8.2-8 9-4.6-.8-8-4.5-8-9V6z M9 12l2 2 4-4',
    undo: 'M9 14L4 9l5-5 M4 9h10a6 6 0 0 1 0 12h-3',
    redo: 'M15 14l5-5-5-5 M20 9H10a6 6 0 0 0 0 12h3',
    moon: 'M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z',
    sun: 'M16 12a4 4 0 1 1-8 0a4 4 0 1 1 8 0 M12 2v2 M12 20v2 M4.9 4.9l1.4 1.4 M17.7 17.7l1.4 1.4 M2 12h2 M20 12h2 M4.9 19.1l1.4-1.4 M17.7 6.3l1.4-1.4',
    combine: 'M3.5 7h5A1.5 1.5 0 0 1 10 8.5v7A1.5 1.5 0 0 1 8.5 17h-5A1.5 1.5 0 0 1 2 15.5v-7A1.5 1.5 0 0 1 3.5 7z M15.5 7h5A1.5 1.5 0 0 1 22 8.5v7a1.5 1.5 0 0 1-1.5 1.5h-5a1.5 1.5 0 0 1-1.5-1.5v-7A1.5 1.5 0 0 1 15.5 7z M10 12h4',
    check: 'M5 12l5 5 9-10',
    upload: 'M12 16V4 M7 9l5-5 5 5 M4 20h16',
    trim: 'M6 4v16 M18 4v16 M6 8H3 M18 16h3 M10 8h4a1 1 0 0 1 1 1v6a1 1 0 0 1-1 1h-4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z',
    cut: 'M9 6a3 3 0 1 1-6 0a3 3 0 1 1 6 0 M9 18a3 3 0 1 1-6 0a3 3 0 1 1 6 0 M20 4L8.1 15.9 M14.5 14.5L20 20 M8.1 8.1L12 12',
    crop: 'M6 2v14a2 2 0 0 0 2 2h14 M18 22V8a2 2 0 0 0-2-2H2',
    text: 'M5 7V5h14v2 M12 5v14 M9 19h6',
    caption: 'M5 4h14a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z M3 15h18 M7 18h6',
    frame: 'M3 8a2 2 0 0 1 2-2h2l2-2h6l2 2h2a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z M16 13a4 4 0 1 1-8 0a4 4 0 1 1 8 0',
    lock: 'M6 11h12a2 2 0 0 1 2 2v6a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-6a2 2 0 0 1 2-2z M8 11V7a4 4 0 0 1 8 0v4',
    plus: 'M12 5v14 M5 12h14',
    close: 'M6 6l12 12 M18 6L6 18',
    folder: 'M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z',
    info: 'M21 12a9 9 0 1 1-18 0a9 9 0 1 1 18 0 M12 11v5 M12 8v.01',
    play: 'M8 5v14l11-7z',
    pause: 'M7 5h3v14H7z M14 5h3v14h-3z'
};
