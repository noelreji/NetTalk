#include <ncurses.h>
#include <panel.h>
#include <string.h>

#define MAX_INPUT_LENGTH 1000

int main() {
    // Initialize ncurses
    initscr();              // Start ncurses mode
    cbreak();               // Disable line buffering
    noecho();               // Don't echo input characters
    curs_set(0);            // Hide cursor
    start_color();          // Start color functionality

    // Define color pairs
    init_pair(1, COLOR_RED, COLOR_BLACK);   // Color pair 1: Red text on black background
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // Color pair 2: Green text on black background

    // Get the size of the terminal
    int rows, cols;
    getmaxyx(stdscr, rows, cols); // Get terminal size in rows and columns

    // Define height for the main content window and input window
    int full_win_height = rows - 6; // Leave space for the input window (5 rows)
    int input_win_height = 5;

    // Create the main content window (upper part of the screen)
    WINDOW *full_win = newwin(full_win_height, cols, 0, 0);
    if (full_win == NULL) {
        printw("Error: Could not create window.\n");
        refresh();
        getch();
        endwin();
        return 1;
    }
    box(full_win, 0, 0);

    // Create the input window (bottom part of the screen)
    WINDOW *input_win = newwin(input_win_height, cols, full_win_height, 0);
    if (input_win == NULL) {
        printw("Error: Could not create input window.\n");
        refresh();
        getch();
        endwin();
        return 1;
    }

    // Print some content in the main window
    mvwprintw(full_win, full_win_height / 2, cols / 2 - 7, "Hello, ncurses!");

    // Text for left and right sides
    const char *left_text = "Leftmost Text";
    const char *right_text = "Rightmost Text";

    // Print left and right texts in different colors
    wattron(full_win, COLOR_PAIR(1));
    mvwprintw(full_win, 1, 1, "%s", left_text);  // Print leftmost text in red
    wattroff(full_win, COLOR_PAIR(1));

    wattron(full_win, COLOR_PAIR(2));
    mvwprintw(full_win, 2, cols - strlen(right_text) - 2, "%s", right_text); // Print rightmost text in green
    wattroff(full_win, COLOR_PAIR(2));

    // Box the input window and add prompt
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 1, "Enter your text: ");

    // Refresh both windows before handling input
    wrefresh(full_win);
    wrefresh(input_win);

    // Buffer to store user input
    char input[MAX_INPUT_LENGTH] = {0};
    int index = 0;

    // Handle real-time input in the input window
    while (1) {
        int ch = wgetch(input_win); // Get a single character input

        if (ch == 10) { // Enter key pressed
            break;
        } else if (ch == 127 || ch == KEY_BACKSPACE) { // Backspace key pressed
            if (index > 0) {
                input[--index] = '\0'; // Remove last character from the buffer
                mvwprintw(input_win, 2, 1, "%-*s", cols - 6, input); // Clear and reprint text
                wmove(input_win, 2, index + 1); // Move cursor to the correct position
            }
        } else if (index < MAX_INPUT_LENGTH - 1 && ch >= 32 && ch <= 126) { // Printable characters
            input[index++] = (char)ch; // Add character to buffer
            input[index] = '\0'; // Null-terminate the string
            mvwprintw(input_win, 2, 1, "%-*s", cols - 6, input); // Clear and reprint text
            wmove(input_win, 2, index + 1); // Move cursor to the correct position
        }
        wrefresh(input_win); // Refresh the input window to show changes
    }

    // Final refresh of both windows
    wrefresh(full_win);
    wrefresh(input_win);

    // Wait for user input to exit
    getch();

    // Clean up
    delwin(input_win);  // Delete the input window
    delwin(full_win);   // Delete the full window
    endwin();           // End ncurses mode

    return 0;
}
