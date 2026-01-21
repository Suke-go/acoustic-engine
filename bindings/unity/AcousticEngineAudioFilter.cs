using UnityEngine;

[RequireComponent(typeof(AudioSource))]
public class AcousticEngineAudioFilter : MonoBehaviour
{
    [Range(0.1f, 1000f)] public float distance = 10f;
    [Range(0f, 1f)] public float roomSize = 0.5f;
    public string scenario = "deep_sea";
    [Range(0f, 1f)] public float scenarioIntensity = 1f;

    private AcousticEngine _engine;
    private string _lastScenario;
    private float _lastScenarioIntensity;

    private void OnEnable()
    {
        _engine = new AcousticEngine();
        _lastScenario = null;
    }

    private void OnDisable()
    {
        if (_engine != null)
        {
            _engine.Dispose();
            _engine = null;
        }
    }

    private void Update()
    {
        if (_engine == null)
            return;

        _engine.Distance = distance;
        _engine.RoomSize = roomSize;

        if (scenario != _lastScenario || !Mathf.Approximately(scenarioIntensity, _lastScenarioIntensity))
        {
            if (!string.IsNullOrEmpty(scenario))
            {
                _engine.SetScenario(scenario, scenarioIntensity);
            }
            _lastScenario = scenario;
            _lastScenarioIntensity = scenarioIntensity;
        }
    }

    private void OnAudioFilterRead(float[] data, int channels)
    {
        if (_engine != null)
        {
            _engine.Process(data, channels);
        }
    }
}
