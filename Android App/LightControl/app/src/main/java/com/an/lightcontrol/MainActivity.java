package com.an.lightcontrol;

import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import okhttp3.*;
import okhttp3.logging.HttpLoggingInterceptor;
import java.io.IOException;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "ESP8266Control";
    private static final String BASE_URL = "http://192.168.29.17";
    private OkHttpClient client;
    private TextView statusText;
    private Button[] relayButtons = new Button[4];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Setup logging interceptor
        HttpLoggingInterceptor logging = new HttpLoggingInterceptor(message ->
                Log.d(TAG, message));
        logging.setLevel(HttpLoggingInterceptor.Level.BODY);

        client = new OkHttpClient.Builder()
                .addInterceptor(logging)
                .build();

        statusText = findViewById(R.id.statusText);

        // Initialize buttons
        relayButtons[0] = findViewById(R.id.relay1Button);
        relayButtons[1] = findViewById(R.id.relay2Button);
        relayButtons[2] = findViewById(R.id.relay3Button);
        relayButtons[3] = findViewById(R.id.relay4Button);

        for (int i = 0; i < relayButtons.length; i++) {
            final int relayNumber = i + 1;
            relayButtons[i].setOnClickListener(v -> toggleRelay(relayNumber));
        }
    }

    private void toggleRelay(int relayNumber) {
        Log.d(TAG, "Attempting to toggle relay " + relayNumber);

        Request request = new Request.Builder()
                .url(BASE_URL + "/relay/" + relayNumber)
                .build();

        Log.d(TAG, "Sending request to: " + request.url());

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Network error: " + e.getMessage(), e);
                runOnUiThread(() -> {
                    String errorMessage = "Connection Error: " + e.getMessage();
                    statusText.setText(errorMessage);
                    Log.d(TAG, "UI updated with error: " + errorMessage);
                    relayButtons[relayNumber-1].setEnabled(true);
                });
            }

            @Override
            public void onResponse(Call call, Response response) {
                Log.d(TAG, "Response received. Code: " + response.code());
                runOnUiThread(() -> {
                    if (response.isSuccessful()) {
                        String message = "Relay " + relayNumber + " toggled";
                        statusText.setText(message);
                        Log.d(TAG, message);
                    } else {
                        String error = "Error: " + response.code();
                        statusText.setText(error);
                        Log.e(TAG, error);
                    }
                    relayButtons[relayNumber-1].setEnabled(true);
                });
            }
        });

        relayButtons[relayNumber-1].setEnabled(false);
    }
}