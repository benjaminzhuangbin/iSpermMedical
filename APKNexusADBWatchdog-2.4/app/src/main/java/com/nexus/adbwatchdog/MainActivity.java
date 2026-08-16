package com.nexus.adbwatchdog;

import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Development / status UI only. Core logic runs in {@link NexusADBWatchdogService}.
 */
public class MainActivity extends AppCompatActivity {

    private TextView tvRoot;
    private TextView tvStatus;
    private TextView tvLog;
    private final Handler uiHandler = new Handler();
    private final Runnable refreshTask = new Runnable() {
        @Override
        public void run() {
            refreshViews();
            uiHandler.postDelayed(this, 3000);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvRoot = findViewById(R.id.tvRoot);
        tvStatus = findViewById(R.id.tvStatus);
        tvLog = findViewById(R.id.tvLog);

        Button btnStart = findViewById(R.id.btnStart);
        Button btnStop = findViewById(R.id.btnStop);
        Button btnRefresh = findViewById(R.id.btnRefresh);
        Button btnStopAdbd = findViewById(R.id.btnTestStopAdbd);
        Button btnBreakPort = findViewById(R.id.btnTestBreakPort);

        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this, "Start Watchdog Service", Toast.LENGTH_SHORT).show();
                refreshViews();
            }
        });
        btnStop.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                NexusADBWatchdogService.stop(MainActivity.this);
                Toast.makeText(MainActivity.this, "Stop requested", Toast.LENGTH_SHORT).show();
            }
        });
        btnRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                refreshViews();
            }
        });

        // DEVELOPMENT / TEST ONLY
        btnStopAdbd.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(MainActivity.this, NexusADBWatchdogService.class);
                i.setAction(WatchdogConfig.ACTION_INJECT);
                i.putExtra(WatchdogConfig.EXTRA_INJECT, "STOP_ADBD");
                startService(i);
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this,
                        "TEST ONLY: STOP_ADBD inject queued", Toast.LENGTH_LONG).show();
            }
        });
        btnBreakPort.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent i = new Intent(MainActivity.this, NexusADBWatchdogService.class);
                i.setAction(WatchdogConfig.ACTION_INJECT);
                i.putExtra(WatchdogConfig.EXTRA_INJECT, "BREAK_PORT");
                startService(i);
                NexusADBWatchdogService.start(MainActivity.this);
                Toast.makeText(MainActivity.this,
                        "TEST ONLY: BREAK_PORT inject queued", Toast.LENGTH_LONG).show();
            }
        });

        // Auto-start service when opening app (convenient for product testing)
        NexusADBWatchdogService.start(this);
        refreshViews();
    }

    @Override
    protected void onResume() {
        super.onResume();
        uiHandler.post(refreshTask);
    }

    @Override
    protected void onPause() {
        uiHandler.removeCallbacks(refreshTask);
        super.onPause();
    }

    private void refreshViews() {
        boolean root = RootShell.hasRoot();
        tvRoot.setText("ROOT=" + (root ? "YES" : "NO (Watchdog needs su)")
                + "\nfilesDir=" + getFilesDir().getAbsolutePath());
        String status = StatusStore.readStatus(this);
        tvStatus.setText(status.isEmpty() ? "(no status yet — start Service)" : status);
        String log = StatusStore.readLogTail(this, 40);
        tvLog.setText(log.isEmpty() ? "(no log yet)" : log);
    }
}
