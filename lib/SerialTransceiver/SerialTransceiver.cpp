#include "SerialTransceiver.h"

SerialTransceiver::SerialTransceiver(Matrix<3> *desiredVelocity, Matrix<4> *jointAngles, Matrix<4> *jointVelocities, Matrix<4> *jointTicks, Matrix<3> *odomPose)
    :desiredVelocity(desiredVelocity), jointAngles(jointAngles), jointVelocities(jointVelocities), jointTicks(jointTicks), odomPose(odomPose)
{
    // Text-based protocol, no buffers needed
}

void SerialTransceiver::rx()
{
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\r') {
            continue;  // ignore CR
        }

        if (c == '\n') {
            // end of line
            rx_buf[rx_len] = '\0';
            rx_len = 0;

            String line = String(rx_buf);
            line.trim();

            if (!line.startsWith("SET "))
                return;

            char buf[64];
            line.toCharArray(buf, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *token = strtok(buf, " ");
            if (!token || strcmp(token, "SET") != 0)
                return;

            float v[4] = {0, 0, 0, 0};
            for (int i = 0; i < 4; ++i) {
                token = strtok(nullptr, " ");
                if (!token) return;
                v[i] = atof(token);
            }

            (*jointVelocities)(0, 0) = v[0];
            (*jointVelocities)(1, 0) = v[1];
            (*jointVelocities)(2, 0) = v[2];
            (*jointVelocities)(3, 0) = v[3];

            return;  // processed one full line this call
        }

        // regular character
        if (rx_len < RX_BUF_SIZE - 1) {
            rx_buf[rx_len++] = c;
        } else {
            // overflow, reset buffer
            rx_len = 0;
        }
    }
}


void SerialTransceiver::tx()
{
    long t_fl = (long)(*jointTicks)(0, 0);
    long t_fr = (long)(*jointTicks)(1, 0);
    long t_rr = (long)(*jointTicks)(2, 0);
    long t_rl = (long)(*jointTicks)(3, 0);
    
    Serial.print("ENC ");
    Serial.print(t_fl);
    Serial.print(" ");
    Serial.print(t_fr);
    Serial.print(" ");
    Serial.print(t_rr);
    Serial.print(" ");
    Serial.println(t_rl);
}

void SerialTransceiver::talk()
{
    rx();
    tx();
}