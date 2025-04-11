import cv2
import dlib
import time
import pygame
import serial
import numpy as np
from scipy.spatial import distance as dist

# Serial communication setup (Ensure correct COM port)
ser = serial.Serial('COM8', 115200, timeout=1)

# Initialize Pygame for alarm sound
pygame.mixer.init()
pygame.mixer.music.load("C:\\Users\\Yeshwanth\\Downloads\\music.wav")  # Ensure file exists

# Eye Aspect Ratio (EAR) calculation
def eye_aspect_ratio(eye):
    A = dist.euclidean(eye[1], eye[5])
    B = dist.euclidean(eye[2], eye[4])
    C = dist.euclidean(eye[0], eye[3])
    ear = (A + B) / (2.0 * C)
    return ear

# Load face detector and landmark predictor
detector = dlib.get_frontal_face_detector()
predictor = dlib.shape_predictor("C:\\dlib_models\\shape_predictor_68_face_landmarks.dat")  # Ensure correct path

# Eye landmark indices
LEFT_EYE = list(range(42, 48))
RIGHT_EYE = list(range(36, 42))

# Thresholds and counters
EAR_THRESHOLD = 0.25
FRAME_THRESHOLD = 10  # Number of consecutive frames with low EAR to trigger drowsiness
counter = 0
alarm_on = False
last_signal_sent = None  # To prevent redundant serial writes

# Open webcam
cap = cv2.VideoCapture(0)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = detector(gray)

    if len(faces) == 0:  # No face detected
        if last_signal_sent != b'1':
            ser.write(b'1')  # Turn motor OFF (No user detected)
            print("Sent signal: 1 (User not in frame, Motor OFF)")
            last_signal_sent = b'1'
    else:
        signal_sent = None
        for face in faces:
            landmarks = predictor(gray, face)
            left_eye = np.array([(landmarks.part(n).x, landmarks.part(n).y) for n in LEFT_EYE])
            right_eye = np.array([(landmarks.part(n).x, landmarks.part(n).y) for n in RIGHT_EYE])

            left_ear = eye_aspect_ratio(left_eye)
            right_ear = eye_aspect_ratio(right_eye)
            avg_ear = (left_ear + right_ear) / 2.0

            # Draw eyes
            cv2.polylines(frame, [left_eye], True, (0, 255, 0), 1)
            cv2.polylines(frame, [right_eye], True, (0, 255, 0), 1)

            # Drowsiness detection logic
            if avg_ear < EAR_THRESHOLD:
                counter += 1
                if counter >= FRAME_THRESHOLD:
                    if not alarm_on:
                        pygame.mixer.music.play(-1)
                        alarm_on = True
                    signal_sent = b'1'  # Motor ON (Drowsy)
                    cv2.putText(frame, "DROWSINESS ALERT!", (30, 150), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 3)
            else:
                counter = 0
                pygame.mixer.music.stop()
                alarm_on = False
                signal_sent = b'0'  # Motor OFF (Awake)

        # Send the signal only if it's different from the last sent signal
        if signal_sent and signal_sent != last_signal_sent:
            ser.write(signal_sent)
            print(f"Sent signal: {signal_sent.decode()}")
            last_signal_sent = signal_sent

    # Display the frame
    cv2.imshow("Drowsiness Detection", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# Cleanup
cap.release()
cv2.destroyAllWindows()
ser.close()
