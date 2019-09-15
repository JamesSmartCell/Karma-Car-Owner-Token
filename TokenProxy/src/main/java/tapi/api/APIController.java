package tapi.api;

import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.bind.annotation.RequestParam;

import java.util.HashMap;
import java.util.Map;

@Controller
@RequestMapping("/api")
public class APIController
{
    private class APIData
    {
        public String fullApiCall;
        public String response;

        public APIData(String call)
        {
            fullApiCall = call;
            response = "";
        }
    }

    private Map<String, APIData> responses = new HashMap<>();

    @Autowired
    public APIController()
    {

    }

    @RequestMapping(value = "/getChallenge", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity pushCheck()
    {
        responses.put("getChallenge", new APIData("getChallenge"));
        String response = awaitResponse("getChallenge");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/checkSignature", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity checkSig(@RequestParam(value = "sig") String sig)
    {
        String pushString = "checkSignature?sig=" + sig;
        responses.put("checkSignature", new APIData(pushString));
        String response = awaitResponse("checkSignature");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/allForward", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity fwd(@RequestParam(value = "addr") String addr)
    {
        String pushString = "allForward?addr=" + addr;
        responses.put("allForward", new APIData(pushString));
        String response = awaitResponse("allForward");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/turnLeft", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity left(@RequestParam(value = "addr") String addr)
    {
        String pushString = "turnLeft?addr=" + addr;
        responses.put("turnLeft", new APIData(pushString));
        String response = awaitResponse("turnLeft");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/turnRight", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity right(@RequestParam(value = "addr") String addr)
    {
        String pushString = "turnRight?addr=" + addr;
        responses.put("turnRight", new APIData(pushString));
        String response = awaitResponse("turnRight");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/backwards", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity back(@RequestParam(value = "addr") String addr)
    {
        String pushString = "backwards?addr=" + addr;
        responses.put("backwards", new APIData(pushString));
        String response = awaitResponse("backwards");

        return new ResponseEntity<>(response, HttpStatus.CREATED);
    }

    private String awaitResponse(String getChallenge)
    {
        String response = null;

        while (response == null)
        {
            APIData rcv = responses.get(getChallenge);
            if (rcv.response.length() > 0)
            {
                response = rcv.response;
                responses.remove(getChallenge);
            }
        }

        return response;
    }

    @RequestMapping(value = "/pollServer", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity pollSvr()
    {
        String rq = null;
        for (String query : responses.keySet())
        {
            APIData rsp = responses.get(query);
            if (rsp.response.length() == 0)
            {
                rq = rsp.fullApiCall;
                break;
            }
        }

        return new ResponseEntity<>(rq, HttpStatus.CREATED);
    }

    @RequestMapping(value = "/replySvr", method = { RequestMethod.GET, RequestMethod.POST })
    public ResponseEntity replySvr(@RequestParam(value = "rq") String rq,
                                   @RequestParam(value = "reply") String reply)
    {
        if (responses.containsKey(rq))
        {
            APIData data = new APIData(rq);
            data.response = reply;
            responses.put(rq, data);
        }
        return new ResponseEntity<>(rq, HttpStatus.CREATED);
    }
}
