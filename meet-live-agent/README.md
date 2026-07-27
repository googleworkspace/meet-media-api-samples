# Meet Live Agent - Multimodal AI in Google Meet

This sample project demonstrates how to integrate Google Meet with Gemini Live to create a multimodal AI agent that can participate in a meeting. The agent can listen to participants, see the meeting video, and respond in real-time with audio. It also provides live transcription and scene description in the Meet side panel.

> [!NOTE]
> The Google Meet Add-on SDK, the Meet Media API, and the Gemini Live model `gemini-3.1-flash-live-preview` are all currently in preview. You need to request access to the [Google Workspace Developer Preview Program (DPP)](https://developers.google.com/workspace/preview).

## Design Overview

The application consists of a frontend built with Lit web components and a Node.js backend.

-   **Frontend**: Uses the `@googleworkspace/meet-addons` SDK to integrate with Google Meet and the `@google/genai` SDK to connect to Gemini Live. It captures audio and video from the meeting via the Meet Media API and streams it to Gemini.
-   **Backend**: An Express server that serves the static frontend files and acts as a secure reverse proxy for Gemini API calls (both HTTP and WebSockets). This allows the application to use the Gemini API without exposing the API key in the browser. It automatically injects an interceptor script to route SDK calls through the proxy.

## Main Features

-   **Real-time Bidirectional Audio**: Speak to Gemini and hear it respond in real-time within the meeting.
-   **Visual Grounding**: The agent receives video frames from the meeting, allowing it to "see" and comment on what's happening.
-   **Live Transcription**: Displays transcripts of what participants say and what Gemini says.
-   **Scene Description**: Periodically generates a description of the visual scene using `gemini-2.5-flash`.
-   **Secure Proxy**: Protects your Gemini API key by routing requests through the backend.

## Prerequisites

Before you begin, ensure you have:

1.  **Google Cloud Project**: A project with billing enabled that has been granted access to the Meet Media API Developer Preview Program (DPP).
2.  **gcloud CLI**: Installed and authenticated. [Install guide](https://cloud.google.com/sdk/docs/install).
3.  **Gemini API Key**: Get one from [Google AI Studio](https://aistudio.google.com/).
4.  **Google Workspace Account**: With permissions to create and use Meet Add-ons.
5.  **Node.js**: Version >22 (required if you plan to build locally).

---

## Deployment Instructions

Follow these steps to deploy the Meet Live Agent as a Google Meet add-on.

### 1. Enable Required APIs
 
 Enable the necessary Google Cloud APIs using the command line:
 
 ```bash
 gcloud services enable meet.googleapis.com \
                        artifactregistry.googleapis.com \
                        run.googleapis.com \
                        cloudbuild.googleapis.com \
                        appsmarket.googleapis.com \
                        appsmarket-component.googleapis.com \
                        gsuiteaddons.googleapis.com
 ```

### 2. Configure OAuth

Before creating the client, you need to configure branding:

1.  Go to the **APIs & Services > OAuth consent screen** page in the Google Cloud Console.
2.  Click **Get started**
3.  Set **App name** to **Meet Live Agent** and **User support email** to your support email, then click **Next**.
4.  Select **Internal** for the User Type (this is sufficient for testing within your organization), then click **Next**.
5.  Set **Email addresses** to your support email, then click **Next**.
6.  Review and check **I agree to the Google API Services: User Data Policy**, then click **Continue** and **Create**.

Then you need to set the data access for the OAuth:

1.  Navigate to **Data Access**.
2.  Click **Add or remove scopes**.
3.  Under **Manually add scopes**, paste the following: `https://www.googleapis.com/auth/meetings.space.readonly https://www.googleapis.com/auth/meetings.conference.media.readonly`
4.  Click **Add to table**, **Update** and **Save**.

### 3. Create OAuth Client

To allow the add-on to authenticate with the Meet Media API:

1.  Go to the **APIs & Services > Credentials** page in the Google Cloud Console.
2.  Navigate to **Clients**.
3.  Click **+ Create client**.
4.  Select **Web application** as the application type.
5.  Set **Name** to `Meet Live Agent`.
6.  Click **Create**.
7.  Note the **Client ID**.

### 4. Configure Environment Variables

Copy the sample environment file and fill in your details:

```bash
cp sample.env .env
```

Edit the `.env` file and provide values for:

*   `PROJECT_ID`: Your Google Cloud Project ID.
*   `REGION`: The region to deploy to (e.g., `us-central1`).
*   `GEMINI_API_KEY`: Your Gemini API key from AI Studio.
*   `CLOUD_PROJECT_NUMBER`: Your Google Cloud Project Number (found in Project Settings).
*   `CLIENT_ID`: Your OAuth 2.0 Client ID (see step 3).

You can use the following commands to retrieve some of the required values:

```bash
# Get Project ID
gcloud config get-value project

# Get Project Number
gcloud projects describe $(gcloud config get-value project) --format="value(projectNumber)"
```

### 5. Deploy to Cloud Run

1.  Run the provided deployment script. This script builds the Docker image and deploys it to Cloud Run, passing the environment variables securely.

    ```bash
    chmod +x deploy.sh
    ./deploy.sh
    ```

2.  Once the deployment completes, the script will output the URL of your Cloud Run service, copy it.

### 6. Update OAuth Redirect URIs

1.  Go back to the **APIs & Services > Credentials** page in the Google Cloud Console.
2.  Edit the OAuth client you initialized and named **Meet Live Agent** in Step 3.
3.  Add the Cloud Run URL you copied in Step 5 to the **Authorized JavaScript origins** list by clicking **+ Add URI**.
4.  Click **Save**.

### 7. Configure Google Workspace Add-on and Marketplace SDK
 
 To make the app appear in Google Meet, you need to configure both the Workspace Add-on deployment and the Marketplace SDK.
 
 #### 7.1 Configure Google Workspace Add-on (HTTP Deployment)
 
 1.  Open the `deployment.json` file in the root of the project.
 2.  Update the `addOnOrigins` and `sidePanelUrl` fields, replacing the placeholder `https://YOUR_CLOUD_RUN_URL` with your actual Cloud Run service URL (obtained in Step 5).
 3.  Run the following command to create the deployment using the `gcloud` CLI:
 
     ```bash
     gcloud workspace-add-ons deployments create meet-live-agent \
         --deployment-file=deployment.json
     ```
 
 4.  The **Deployment ID** will be `meet-live-agent`. You will need this in the next step.
 
 #### 7.2 Configure Google Workspace Marketplace SDK
 
 1.  Search and select **Google Workspace Marketplace SDK** in the Google Cloud Console.
 2.  Click **Manage** then select the **App Configuration** tab.
 3.  Set **App Visibility** to **Private** for testing.
 4.  Set **Installation Settings** to **Individual + Admin Install**.
 5.  Under **App Integrations** select **Google Workspace add-on**, select **HTTP or other deployments**, and select the deployment ID **meet-live-agent**.
 6.  Under **Developer Information**, set the **Developer Name**, **Developer Website URL**, and **Developer Email** to your own information.
 7.  Click **Save Draft**.
 
 #### 7.3 Install the Add-on Deployment
 
 To install the add-on for your account so you can see it in Google Meet, run the following command:
 
 ```bash
 gcloud workspace-add-ons deployments install meet-live-agent
 ```
 
 ## Testing the Add-on in Google Meet

After completing the deployment and configuration, you can test the add-on in a live meeting:

1.  Go to [Google Meet](https://meet.google.com) and start a new **instant meeting**.
2.  Click the **Meeting tools** icon in the bottom right corner then select the **Add-ons** tab.
3.  You should see your add-on **Meet Live Agent** listed as installed.
4.  Click on it to open the side panel.
5.  Click **Connect to Meet Media API** to start the agent.
6.  Go through the OAuth flow and grant all the permissions requested by the add-on.
7.  Click **Connect to Meet Media API** in the side panel.
8.  Click **Start Meet Live Agent** in the pop-up window to share audio and video of the meeting to the add-on.
9.  The side panel should display real-time audio volume and transcripts, and a scene description.
10. You can talk and present in the meeting to test interacting with Gemini Live.

## Building Locally
 
 If you want to build the project locally (e.g., to verify the build before deploying):
 
 1.  Install the dependencies:
     ```bash
     npm install
     ```
 2.  Run the build command:
     ```bash
     npm run build
     ```
 
 This will generate the static assets in the `dist` directory.
 
 *Note: You do not need to build locally to deploy, as the `deploy.sh` script triggers Cloud Build to handle the build process in the cloud.*
