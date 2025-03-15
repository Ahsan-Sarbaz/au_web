// a k6 test that sends a request to the web server
import http from "k6/http";
import { check, sleep } from "k6";


export const options = {
    // iterations: 10000,
    vus: 1300,
    duration: "30s",
};

export default function () {
    const response = http.get('http://localhost:8080/');

    check(response, {
        "is status 200": (r) => r.status === 200,
    });

    sleep(0.2);
}