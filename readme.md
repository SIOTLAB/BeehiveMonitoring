# Beehive Monitoring

A system for Beehive monitoring that provides data logging and abnormal data detection.

This project consists of 3 parts
- `web-app` for UI
- `cv-code` for measure bee activity through computer vision
- `ml-code` to perform data analysis

Further information can be found in each components' readme.

## Deployment

To deploy this project, `cv-code` and `ml-code` should be downloaded to the edge and configured to auto start according to `ml-code/readme.md`  

Web app use MongoDB in combination with ExpressJS and ReactJS. The frontend and backend can be deployed according to standard practices and MongoDB path can be configured by `.env` file. Further information are provided in `web-app/README.md`
