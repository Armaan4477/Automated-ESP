package com.an.lightcontrol;

import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.TextView;
import android.widget.ToggleButton;
import androidx.appcompat.app.AppCompatActivity;
import okhttp3.*;
import okhttp3.logging.HttpLoggingInterceptor;
import org.json.JSONObject;
import java.io.IOException;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "ESP8266Control";
    private static final String BASE_URL = "http://192.168.29.17";
    private OkHttpClient client;
    private TextView statusText;
    private TextView timeText;
    private ToggleButton[] relayButtons = new ToggleButton[4];
    private final Handler handler = new Handler();
    private static final int UPDATE_INTERVAL = 1000; // 1 second

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
        timeText = findViewById(R.id.timeText);

        // Initialize buttons
        relayButtons[0] = findViewById(R.id.relay1Button);
        relayButtons[1] = findViewById(R.id.relay2Button);
        relayButtons[2] = findViewById(R.id.relay3Button);
        relayButtons[3] = findViewById(R.id.relay4Button);

        for (int i = 0; i < relayButtons.length; i++) {
            final int relayNumber = i + 1;
            relayButtons[i].setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (buttonView.isPressed()) { // Only trigger if user pressed the button
                    toggleRelay(relayNumber);
                }
            });
        }

        // Start periodic updates
        startPeriodicUpdates();
    }

    private void startPeriodicUpdates() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                updateTime();
                updateRelayStates();
                handler.postDelayed(this, UPDATE_INTERVAL);
            }
        }, UPDATE_INTERVAL);
    }

    private void updateTime() {
        Request request = new Request.Builder()
                .url(BASE_URL + "/time")
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Time update failed: " + e.getMessage());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    final String time = response.body().string();
                    runOnUiThread(() -> timeText.setText("Time: " + time));
                }
            }
        });
    }

    private void updateRelayStates() {
        Request request = new Request.Builder()
                .url(BASE_URL + "/relay/status")
                .build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Status update failed: " + e.getMessage());
            }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    try {
                        JSONObject states = new JSONObject(response.body().string());
                        runOnUiThread(() -> {
                            try {
                                for (int i = 0; i < relayButtons.length; i++) {
                                    boolean state = states.getBoolean(String.valueOf(i + 1));
                                    relayButtons[i].setChecked(state);
                                }
                            } catch (Exception e) {
                                Log.e(TAG, "Error parsing relay states", e);
                            }
                        });
                    } catch (Exception e) {
                        Log.e(TAG, "Error parsing response", e);
                    }
                }
            }
        });
    }

    private void toggleRelay(int relayNumber) {
        Request request = new Request.Builder()
                .url(BASE_URL + "/relay/" + relayNumber)
                .post(RequestBody.create(null, new byte[0]))
                .build();

        relayButtons[relayNumber-1].setEnabled(false);

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) {
                Log.e(TAG, "Network error: " + e.getMessage(), e);
                runOnUiThread(() -> {
                    statusText.setText("Connection Error: " + e.getMessage());
                    relayButtons[relayNumber-1].setEnabled(true);
                    updateRelayStates(); // Refresh states
                });
            }

            @Override
            public void onResponse(Call call, Response response) {
                runOnUiThread(() -> {
                    relayButtons[relayNumber-1].setEnabled(true);
                    if (response.isSuccessful()) {
                        statusText.setText("Relay " + relayNumber + " toggled");
                    } else {
                        statusText.setText("Error: " + response.code());
                        updateRelayStates(); // Refresh states
                    }
                });
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
    }
}