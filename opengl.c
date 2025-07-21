#include "local.h"
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define TEXT_FONT GLUT_BITMAP_HELVETICA_18
#define BAR_WIDTH 0.15f
#define BAR_HEIGHT_FACTOR 0.8f
#define MAX_BAR_VALUE 100.0f
#define PANEL_MARGIN 0.05f

SharedStats *shmptr_stats;
FileMetadata *shmptr_metaData;
Config *shmptr_config;
int shmid_stats, shmid_config, shmid_metaData;

void cleanup();
void sigtermHandler(int sig);
void initializeIPCS();
void renderBitmapString(float x, float y, void *font, const char *string);
void displayAllStatisticsText();
void displayFileAverages();
void displayBarGraph(float x, float y, float width, float height, float value, float maxValue, const char *label, float r, float g, float b);
void display();
void timer(int value);

void cleanup() {
    if (shmptr_stats != NULL) {
        shmdt(shmptr_stats);
        shmptr_stats = NULL;
    }
    if (shmptr_config != NULL) {
        shmdt(shmptr_config);
        shmptr_config = NULL;
    }
    if (shmptr_metaData != NULL) {
        shmdt(shmptr_metaData);
        shmptr_metaData = NULL;
    }


}

void sigtermHandler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        cleanup();
        exit(0);
    }
}

void initializeIPCS() {
    key_t key;

    key = ftok("./", 'S');
    if (key == -1) {
        perror("ftok for SharedStats failed");
        exit(EXIT_FAILURE);
    }
    shmid_stats = shmget(key, sizeof(SharedStats), 0666);
    if (shmid_stats == -1) {
        perror("shmget for SharedStats failed");
        exit(EXIT_FAILURE);
    }
    shmptr_stats = (SharedStats *)shmat(shmid_stats, NULL, 0);
    if (shmptr_stats == (void *)-1) {
        perror("shmat for SharedStats failed");
        exit(EXIT_FAILURE);
    }

    key = ftok("./", 'C');
    if (key == -1) {
        perror("ftok for Config failed");
        exit(EXIT_FAILURE);
    }
    shmid_config = shmget(key, sizeof(Config), 0666);
    if (shmid_config == -1) {
        perror("shmget for Config failed");
        exit(EXIT_FAILURE);
    }
    shmptr_config = (Config *)shmat(shmid_config, NULL, 0);
    if (shmptr_config == (void *)-1) {
        perror("shmat for Config failed");
        exit(EXIT_FAILURE);
    }

    // FileMetadata
    key = ftok("./", 'G');
    if (key == -1) {
        perror("ftok for FileMetadata failed");
        exit(EXIT_FAILURE);
    }
    shmid_metaData = shmget(key, sizeof(FileMetadata) * MAX_FILES, 0666);
    if (shmid_metaData == -1) {
        perror("shmget for FileMetadata failed");
        exit(EXIT_FAILURE);
    }
    shmptr_metaData = (FileMetadata *)shmat(shmid_metaData, NULL, 0);
    if (shmptr_metaData == (void *)-1) {
        perror("shmat for FileMetadata failed");
        exit(EXIT_FAILURE);
    }
}

// Function to render bitmap text
void renderBitmapString(float x, float y, void *font, const char *string) {
    const char *c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

// Function to display all statistics as text on the left side
void displayAllStatisticsText() {
    float x = -0.95f;
    float y = 0.9f;
    float line_spacing = 0.05f;
    char buffer[512];

    glColor3f(1.0f, 1.0f, 1.0f); // White color for text

    // Title
    renderBitmapString(x, y, TEXT_FONT, "File Management Statistics");
    y -= line_spacing * 1.5f;

    // Total Generated Files
    snprintf(buffer, sizeof(buffer), "Total Generated Files: %d", shmptr_stats->totalGenerated);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Total Processed Files
    snprintf(buffer, sizeof(buffer), "Total Processed Files: %d", shmptr_stats->totalProcessed);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Total Unprocessed Files
    snprintf(buffer, sizeof(buffer), "Total Unprocessed Files: %d", shmptr_stats->totalUnprocessed);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Total Backed-up Files
    snprintf(buffer, sizeof(buffer), "Total Backed-up Files: %d", shmptr_stats->totalBackedup);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Total Deleted Files
    snprintf(buffer, sizeof(buffer), "Total Deleted Files: %d", shmptr_stats->totalDeleted);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Min Average
    snprintf(buffer, sizeof(buffer), "Min Average: %.2f", shmptr_stats->minAverage);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Max Average
    snprintf(buffer, sizeof(buffer), "Max Average: %.2f", shmptr_stats->maxAverage);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // File with Min Average
    snprintf(buffer, sizeof(buffer), "File with Min Avg: %s (Col %d)", shmptr_stats->minAverageFile, shmptr_stats->minAverageCol);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // File with Max Average
    snprintf(buffer, sizeof(buffer), "File with Max Avg: %s (Col %d)", shmptr_stats->maxAverageFile, shmptr_stats->maxAverageCol);
    renderBitmapString(x, y, TEXT_FONT, buffer);
    y -= line_spacing;

    // Additional statistics can be added here similarly
}

// Function to display file averages on the right side
void displayFileAverages() {
    float x_start = 0.4f;
    float y_start = 0.8f;
    float line_spacing = 0.05f;
    char buffer[512];

    glColor3f(1.0f, 1.0f, 0.0f); // Yellow color for title
    renderBitmapString(x_start, y_start, TEXT_FONT, "File Averages");
    y_start -= line_spacing * 1.5f;

    glColor3f(0.0f, 1.0f, 0.0f); // Green color for min average
    snprintf(buffer, sizeof(buffer), "Min Avg: %.2f", shmptr_stats->minAverage);
    renderBitmapString(x_start, y_start, TEXT_FONT, buffer);
    y_start -= line_spacing;

    glColor3f(1.0f, 0.0f, 0.0f); // Red color for max average
    snprintf(buffer, sizeof(buffer), "Max Avg: %.2f", shmptr_stats->maxAverage);
    renderBitmapString(x_start, y_start, TEXT_FONT, buffer);
    y_start -= line_spacing;

    glColor3f(1.0f, 1.0f, 1.0f); // White color for additional info
    snprintf(buffer, sizeof(buffer), "Min Avg File: %s", shmptr_stats->minAverageFile);
    renderBitmapString(x_start, y_start, TEXT_FONT, buffer);
    y_start -= line_spacing;

    snprintf(buffer, sizeof(buffer), "Max Avg File: %s", shmptr_stats->maxAverageFile);
    renderBitmapString(x_start, y_start, TEXT_FONT, buffer);
    y_start -= line_spacing;

    // Additional file average details can be added here
}

// Function to display a single bar graph
void displayBarGraph(float x, float y, float width, float height, float value, float maxValue, const char *label, float r, float g, float b) {
    // Normalize the bar height based on the maximum value
    float normalized_height = (value / maxValue) * height;
    if (normalized_height > height) {
        normalized_height = height; // Cap the height to the panel
    }

    // Draw the bar
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + normalized_height);
    glVertex2f(x, y + normalized_height);
    glEnd();

    // Draw the border of the bar
    glColor3f(1.0f, 1.0f, 1.0f); // White border
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();

    // Render the label below the bar
    glColor3f(1.0f, 1.0f, 1.0f); // White color for text
    // Calculate label position centered under the bar
    float label_x = x + width / 2.0f - (strlen(label) * 0.005f); // Adjust based on label length
    float label_y = y - 0.05f;
    renderBitmapString(label_x, label_y, TEXT_FONT, label);
}

// Function to display general statistics using bar graphs
void displayStatisticsBars() {
    float x_start = -0.25f;
    float y_start = -0.8f;
    float spacing = 0.2f;
    float current_x = x_start;
    float current_y = -0.8f;
    float max_value = (float)MAX_BAR_VALUE; // Define based on expected maximum

    // Display each statistic as a bar
    displayBarGraph(current_x, current_y, BAR_WIDTH, BAR_HEIGHT_FACTOR, (float)shmptr_stats->totalGenerated, max_value, "Generated", 0.0f, 0.5f, 1.0f);
    current_x += spacing;

    displayBarGraph(current_x, current_y, BAR_WIDTH, BAR_HEIGHT_FACTOR, (float)shmptr_stats->totalProcessed, max_value, "Processed", 0.0f, 1.0f, 0.0f);
    current_x += spacing;

    displayBarGraph(current_x, current_y, BAR_WIDTH, BAR_HEIGHT_FACTOR, (float)shmptr_stats->totalUnprocessed, max_value, "Unprocessed", 1.0f, 0.5f, 0.0f);
    current_x += spacing;

    displayBarGraph(current_x, current_y, BAR_WIDTH, BAR_HEIGHT_FACTOR, (float)shmptr_stats->totalBackedup, max_value, "Backed-up", 0.5f, 0.0f, 0.5f);
    current_x += spacing;

    displayBarGraph(current_x, current_y, BAR_WIDTH, BAR_HEIGHT_FACTOR, (float)shmptr_stats->totalDeleted, max_value, "Deleted", 1.0f, 0.0f, 0.0f);
}

// Display function to update the OpenGL window
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // Render background panels
    // Left Panel for Text
    glColor4f(0.2f, 0.2f, 0.2f, 0.9f); // Semi-transparent dark gray
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(-0.3f, -1.0f);
    glVertex2f(-0.3f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    // Right Panel for Bar Graphs and Averages
    glColor4f(0.1f, 0.1f, 0.1f, 0.9f); // More opaque dark gray
    glBegin(GL_QUADS);
    glVertex2f(-0.3f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-0.3f, 1.0f);
    glEnd();

    // Display all statistics as text on the left side
    displayAllStatisticsText();

    // Display bar graphs on the right side
    displayStatisticsBars();

    // Display file averages on the right side
    displayFileAverages();

    glutSwapBuffers();
}

// Timer function for continuous updates
void timer(int value) {
    glutPostRedisplay();  // Request a redraw
    glutTimerFunc(1000 / 60, timer, 0);  // Call timer again after ~16 ms (60 FPS)
}

// Main function to initialize GLUT and start the application
int main(int argc, char **argv) {
    // Initialize shared memory IPCs
    initializeIPCS();

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("File Management Simulation Statistics");

    // Set background color to dark gray
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    // Setup cleanup and signal handling
    atexit(cleanup);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, sigtermHandler);  // Handle Ctrl+C

    // Register callback functions
    glutDisplayFunc(display);
    glutTimerFunc(1000 / 60, timer, 0);  // Start the timer (update ~60 times per second)

    // Start the main loop
    glutMainLoop();
    return 0;
}
