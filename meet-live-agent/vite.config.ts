import path from 'path';
import { defineConfig, loadEnv } from 'vite';

export default defineConfig(({ mode }) => {
    const env = loadEnv(mode, '.', '');
    console.log("Loaded env:", env);
    return {
      server: {
        port: 3000,
        host: '0.0.0.0',
      },
      plugins: [],
      define: {
        'process.env.GEMINI_API_KEY': JSON.stringify(env.GEMINI_API_KEY),
        'process.env.CLIENT_ID': JSON.stringify(env.CLIENT_ID),
        'process.env.CLOUD_PROJECT_NUMBER': JSON.stringify(env.CLOUD_PROJECT_NUMBER)
      },
      resolve: {
        alias: {
          '@': path.resolve(__dirname, '.'),
        }
      }
    };
});
