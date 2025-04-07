#ifndef HTML_H_
#define HTML_H_

#include "config.h"
#include "Audio.h" // Include the header for streamPlayer
extern Audio audio; // Declare streamPlayer as an external object

const char index_html[] PROGMEM = R"rawliteral(
    <!DOCTYPE HTML><html>
    <head>
      <link rel='icon' href='/favicon.ico' type='image/x-icon'>
      <link rel="shortcut icon" href="/favicon.ico" type="image/x-icon">
      <link rel="apple-touch-icon" sizes="180x180" href="/icon.png">
      <link rel="icon" type="image/png" sizes="192x192" href="/icon.png">
    
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>ESP32 Web Radio</title>
      <style>
        html {font-family: Arial; display: inline-block; text-align: center;}
        h2 {font-size: 2.3rem;}
        p {font-size: 1.1rem;}
        table {border: 1px solid black; border-collapse: collapse; margin: 0px 0px;}
        td, th {font-size: 0.8rem; border: 1px solid gray; border-collapse: collapse;}
        td:hover {font-weight:bold;}
        a {color: black; text-decoration: none;}
        body {max-width: 1380px; margin:0px auto; padding-bottom: 25px;}
        .slider {-webkit-appearance: none; margin: 14px; width: 330px; height: 10px; background: #4CAF50; outline: none; -webkit-transition: .2s; transition: opacity .2s; border-radius: 5px;}
        .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 25px; background: #4a4a4a; cursor: pointer; border-radius: 5px;}
        .slider::-moz-range-thumb { width: 35px; height: 35px; background: #4a4a4a; cursor: pointer; border-radius: 5px;} 
        .button { background-color: #4CAF50; border: 1; color: white; padding: 10px 20px; border-radius: 5px;}
        .buttonBank { background-color: #4CAF50; border: 1; color: white; padding: 8px 8px; border-radius: 5px; width: 35px; height: 35px; margin: 0 1.5px;}
        .buttonBankSelected { background-color: #505050; border: 1; color: white; padding: 8px 8px; border-radius: 5px; width: 35px; height: 35px; margin: 0 1.5px;}
        .buttonBank:active {background-color: #4a4a4a box-shadow: 0 4px #666; transform: translateY(2px);}
        .buttonBank:hover {background-color: #4a4a4a;}
        .button:hover {background-color: #4a4a4a;}
        .button:active {background-color: #4a4a4a; box-shadow: 0 4px #666; transform: translateY(2px);}
        .column { align: center; padding: 5px; display: flex; justify-content: space-between;}
        .stationList {text-align:left; margin-top: 0px; width: 280px; margin-bottom:0px;cursor: pointer;}
          .stationNumberList {text-align:center; margin-top: 0px; width: 35px; margin-bottom:0px;}
          .stationListSelected {text-align:left; margin-top: 0px; width: 280px; margin-bottom:0px;cursor: pointer; background-color: #4CAF50;}
          .stationNumberListSelected {text-align:center; margin-top: 0px; width: 35px; margin-bottom:0px; background-color: #4CAF50;}
      </style>
    </head>
    
    <body>
      <h2>ESP32 Web Radio</h2>
      <p style="font-size: 1rem;">Station:%STATIONNUMBER%   Bank:%BANKVALUE%</p>
      <p style="font-size: 1.6rem;"><span id="textStationName"><b> %STATIONNAMEVALUE%</b></span></p>
      
      <p>Volume: <span id="textSliderValue">%SLIDERVALUE%</span></p>
      <p><input type="range" onchange="updateSliderVolume(this)" id="volumeSlider" min="1" max="21" value="%SLIDERVALUE%" step="1" class="slider"></p>
    
      <p>Bank selection:</p>
      
         
      <script>
      function updateSliderVolume(element) 
      {
        var sliderValue = document.getElementById("volumeSlider").value;
        document.getElementById("textSliderValue").innerHTML = sliderValue;
        console.log(sliderValue);
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/update?volume="+sliderValue, true);
        xhr.send();
      }
    
      function volume(x) 
      {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/volume" + x, true);
        xhr.send();
        document.location.reload();
      }
    
      function station(x) 
      {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/station" + x, true);
        xhr.send();
        document.location.reload();
      }
      function stationLoad(x) 
      {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/update?station=" + x, false);
        xhr.send();
        document.location.reload();
        window.location.href=window.location.href();
      }
      function bankLoad(x) 
      {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/update?bank=" + x, false);
        xhr.send();
        document.location.reload();
        window.location.href=window.location.href();
      }
      </script>
    
    )rawliteral";

class Html
{
public:
  void webUrlStationPlay();
  void stationBankListHtmlMobile();
  void stationBankListHtmlPC();
};

#endif