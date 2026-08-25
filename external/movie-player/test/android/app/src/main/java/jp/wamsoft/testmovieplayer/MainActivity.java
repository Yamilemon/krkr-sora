package jp.wamsoft.testmovieplayer;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Handler;
import android.os.Bundle;
import android.widget.TextView;

import jp.wamsoft.testmovieplayer.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'testmovieplayer' library on application startup.
    static {
        System.loadLibrary("testmovieplayer");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        MyMoviePlayer.setAssetManager(getAssets());

        // Example of a call to a native method
        // TextView tv = binding.sampleText;
        // tv.setText(String.valueOf(position()) + "/" + String.valueOf(duration()));
    }

    @Override
    protected void onResume() {
        super.onResume();
        StartThread();
    }

    @Override
    protected void onPause() {
        super.onPause();
        StopThread();
    }

    private final Handler m_handler = new Handler();
    private Runnable m_runnable;

    protected void StartThread() {
        m_runnable = new Runnable() {
            TextView tv = binding.sampleText;

            @Override
            public void run() {
                tv.setText(String.format("%12d", binding.testMovieView.position()) + "/"
                        + String.format("%12d", binding.testMovieView.duration()));
                m_handler.postDelayed(this, 10);
            }
        };
        m_handler.post(m_runnable);
    }

    protected void StopThread() {
        m_handler.removeCallbacks(m_runnable);
    }
}
