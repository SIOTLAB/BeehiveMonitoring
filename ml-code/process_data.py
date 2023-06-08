import sys
import os
import tensorflow as tf
import numpy as np
import requests
import datetime

# SERVER_BASE_URL='https://beehive-webapp.herokuapp.com/api/data/serial'
# DEVICE_SERIAL='123'
# DEVICE_Lat='37.3018'
# DEVICE_Lang='-122.0432'

# Number of datapoints used for prediction
PRED_TW=24

def main():
    setupGlobalConfig()
    (input_valid, bees_in, bees_out) = checkAndReturnInput()

    os.chdir('/home/pi')
    appendLogsToFile('sdp-beehive.raw-data.log',"[{timestamp}] In: {bees_in}, Out: {bees_out}".format(timestamp=datetime.datetime.now().isoformat(), bees_in=bees_in, bees_out=bees_out))

    current_weather = get_weather_data(DEVICE_Lat, DEVICE_Lang)
    try:
        past_data = downloadPastData(PRED_TW)
    except InsufficientData:
        uploadData(raw_activity=(bees_in,bees_out), weather=current_weather)
        return
    
    current_data = np.array([bees_in, bees_out])
    (next_prediction, last_error) = performPrediction(past_data, current_data, current_weather)
    uploadData(raw_activity=(bees_in,bees_out), weather=current_weather, prediction=next_prediction, dev=last_error)

import io

def setupGlobalConfig():
    global SERVER_BASE_URL
    global DEVICE_SERIAL
    global DEVICE_Lat
    global DEVICE_Lang
    SERVER_BASE_URL=os.environ['SDP_SERVER_BASE_URL']
    DEVICE_SERIAL=os.environ['SDP_DEVICE_SERIAL']
    DEVICE_Lat=os.environ['SDP_DEVICE_Lat']
    DEVICE_Lang=os.environ['SDP_DEVICE_Lang']
    if not SERVER_BASE_URL:
        raise Exception("Environment not configured")
    if not DEVICE_SERIAL:
        raise Exception("Environment not configured")
    if not DEVICE_Lat:
        raise Exception("Environment not configured")
    if not DEVICE_Lang:
        raise Exception("Environment not configured")
    print("ENV: {}, {}, {}, {}".format(SERVER_BASE_URL, DEVICE_SERIAL, DEVICE_Lat, SDP_DEVICE_Lang))

def appendLogsToFile(path, log_string):
    try:
        with open(path, 'a') as file:
            file.write(log_string + '\n')
        print("Log appended successfully.")
    except IOError:
        print("Error: Failed to append log to file.")

def performPrediction(past_data, current_data, current_weather):
    model = create_model()
    
    activities=[]
    weathers=[]
    for dataPt in past_data:
        activities.append((dataPt["raw_activity"]["x"],dataPt["raw_activity"]["y"]))
        weathers.append((dataPt["weather"]["temp"],dataPt["weather"]["humidity"],dataPt["weather"]["windspeed"]))
    activities = np.array(activities)
    weathers = np.array(weathers)

    X_train = np.concatenate((activities[:-2],activities[1:-1],weathers[2:]),axis=1)
    Y_train = activities[2:]
    model.fit(X_train, Y_train, epochs=50, batch_size=64)
    prediction = model.predict(np.concatenate((activities[-1:],[current_data],weathers[-1:]),axis=1))
    last_data_point = activities[-1]
    stddev = np.std(Y_train, axis=0)
    stddev=np.max(np.array([np.array([0.01,0.01]),stddev]), axis=0)
    print(stddev)
    deviation = np.max(np.abs(last_data_point-current_data)/stddev)
    print()
    return (prediction[0].tolist(), deviation)

class ServerError(Exception):
    pass

class InsufficientData(Exception):
    pass

def uploadData(raw_activity, weather, prediction = None, dev = 0):
    """
        Upload data to server
        @param raw_activity Tuple of 2 (bees in, bees out)
        @param weather Tuple of 3 (temperature, humidity, windspeed)
            in Celsius, percent, and mph respectively
        @param prediction Tuple of 2 (bees in, bees out)
        @param dev deviation of last prediction as z score
    """
    # Construct the full URL for the POST request
    url = "{base}/{serial}".format(base=SERVER_BASE_URL, serial=DEVICE_SERIAL)

    # Get current time in ISO format
    current_time = datetime.datetime.now().isoformat()

    data = {
        "time": current_time,
        "raw_activity": {"x": raw_activity[0], "y": raw_activity[1]},
        "weather": {"temp": weather[0], "humidity": weather[1], "windspeed": weather[2]}
    }

    if prediction is not None:
        data["prediction_activity"] = {"x": prediction[0], "y": prediction[1]}
        data["last_prediction_deviation"] = dev

    response = requests.put(url, json=data)
    print("[INFO] Uploading {}".format(data))
    if response.status_code != 200:
        raise ServerError("Failed to upload data: " + str(response.status_code))

def downloadPastData(limit):
    url = "{base}/{serial}?limit={lim}".format(base=SERVER_BASE_URL, serial=DEVICE_SERIAL, lim=str(limit))
    response = requests.get(url)

    if response.status_code == 200:
        data = response.json()
        return data
    elif response.status_code == 204:
        raise InsufficientData("No sufficient data exist")
    else:
        raise ServerError("Failed to download data: " + str(response.status_code))

def get_weather_data(lat, lng):
    """
    Get the temperature, humidity, and wind speed data from the National Weather Service API.

    Args:
        lat (float): The latitude of the location to get weather data for.
        lng (float): The longitude of the location to get weather data for.

    Returns:
        tuple: A tuple of the temperature (float), relative humidity (float), and wind speed (float)
            in Celsius, percent, and mph respectively.

    Example:
        >>> get_future_weather_data(39.7456, -97.0892)
        (21.11111111111111, 60.0, 5.0)
    """
    api_url = "https://api.weather.gov/points/{lat},{lng}".format(lat=lat, lng=lng)
    response = requests.get(api_url)
    weather_data = response.json()
    hourly_data_url = weather_data["properties"]["forecastHourly"]
    hourly_data_response = requests.get(hourly_data_url)
    hourly_data = hourly_data_response.json()
    periods = hourly_data["properties"]["periods"]

    # Get the current time as a Unix timestamp
    now = int(datetime.datetime.now().timestamp())
    future = int((datetime.datetime.now() + datetime.timedelta(hours=1)).timestamp())

    # Initialize the result with the current data
    temperature = periods[0]["temperature"]
    temperature_unit = periods[0]["temperatureUnit"]
    relative_humidity = periods[0]["relativeHumidity"]["value"]
    wind_speed = periods[0]["windSpeed"]

    # Iterate through the periods to find the one that encloses now + 1 hour
    for period in periods:
        start_time = int(datetime.datetime.fromisoformat(period["startTime"]).timestamp())
        end_time = int(datetime.datetime.fromisoformat(period["endTime"]).timestamp())
        if start_time <= future <= end_time:
            temperature = period["temperature"]
            temperature_unit = period["temperatureUnit"]
            relative_humidity = period["relativeHumidity"]["value"]
            wind_speed = period["windSpeed"]
            break

    if temperature_unit == "F":
        temperature = (temperature - 32) * 5 / 9
    wind_speed = float(wind_speed.split(" ")[0])

    return temperature, relative_humidity, wind_speed

def create_model():
    model = tf.keras.Sequential()
    model.add(tf.keras.layers.Dense(128, input_shape=(7,), activation='relu'))
    model.add(tf.keras.layers.Dense(64, activation='relu'))
    model.add(tf.keras.layers.Dense(32, activation='relu'))
    model.add(tf.keras.layers.Dense(16, activation='relu'))
    model.add(tf.keras.layers.Dense(2, activation='linear'))
    model.compile(optimizer='adam', loss='mean_squared_error', metrics=['mae'])
    return model

def checkAndReturnInput():
    """
        Read input from Arguments
        @returns (input valid, bees in, bees out)
    """
    if len(sys.argv) != 3:
        print('Usage: {sys.argv[0]} <num bees enter> <num bees left>')
        return (False,0,0)
    res=(True,0,0)
    try:
        res=(True,int(sys.argv[1]),int(sys.argv[2]))
    except:
        print('Error: Argument 1 & 2 must be integers')
        res = (False,0,0)
    return res

if __name__ == '__main__':
    main()


