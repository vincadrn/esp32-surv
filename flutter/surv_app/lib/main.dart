import 'package:intl/intl.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_storage/firebase_storage.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'firebase_options.dart';
import 'package:flutter_screenutil/flutter_screenutil.dart';
import 'package:gallery_saver/gallery_saver.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Flutter Demo',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  Future<void> loginToFirebase() async {
    try {
      await FirebaseAuth.instance.signInWithEmailAndPassword(
      email: 'xxx', password: 'xxx'
    );
    } on FirebaseAuthException catch (e) {
      print('Error logging in.');
    }
  }

  Future<void> saveTokenToDatabase(String token) async {
    await FirebaseFirestore.instance
      .collection('users')
      .doc('mobile')
      .update({
        'tokens' : token,
      });
  }

  Future<void> setupToken() async {
    String? token = await FirebaseMessaging.instance.getToken();
    await saveTokenToDatabase(token!);
    FirebaseMessaging.instance.onTokenRefresh.listen(saveTokenToDatabase);
  }

  /* Accessing the latest picture */
  Future<Map<String, dynamic>> _getLatestDocument() async {
    QuerySnapshot query = await FirebaseFirestore.instance.collection('logs').orderBy('timestamp', descending: true).limit(1).get();
    return query.docs[0].data() as Map<String, dynamic>;
  }

  Future<String> _addImageDescription() async {
    final data = await _getLatestDocument();
    final String time = DateFormat().format(data['timestamp'].toDate());
    return 'Picture taken on $time';
  }

  Future<String> _downloadImage() async {
    final data = await _getLatestDocument();
    final String path = data['path'];
    final gsReference = FirebaseStorage.instance.refFromURL('gs://embsys-project.appspot.com$path'); // Storage
    return await gsReference.getDownloadURL();
  }

  /* Functionality to save the picture to gallery */
  void _saveImageToGallery() async {
    final url = await _downloadImage();
    if (!kIsWeb) GallerySaver.saveImage(url);
  }

  @override
  void initState() {
    super.initState();
    if (!kIsWeb) {
      setupToken();
    }
    loginToFirebase();
  }

  @override
  Widget build(BuildContext context) {
    ScreenUtil.init(context);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Surveillance System App'),
      ),
      body: Center(
        child: ListView(
          padding: const EdgeInsets.all(8),
          children: [
            FutureBuilder<String>(
              future: _downloadImage(),
              builder: (context, snapshot) {
                if (snapshot.hasData) {
                  return RotatedBox(
                    quarterTurns: 1,
                    child: Image.network(
                      snapshot.data!,
                      fit: BoxFit.cover,
                    ),
                  );
                } else if (snapshot.hasError) {
                  return const Text('Error');
                } else {
                  return const Text('Awaiting...');
                }
              }
            ),
            FutureBuilder<String>(
              future: _addImageDescription(),
              builder: (context, snapshot) {
                late String text;
                if (snapshot.hasData) {
                  text = snapshot.data!;
                } else if (snapshot.hasError) {
                  text = 'Error getting date.';
                } else {
                  text = 'Awaiting...';
                }
                return Container(
                  padding: const EdgeInsets.only(bottom: 16, top: 16),
                  child: Text(
                    text,
                    style: const TextStyle(
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                );
              }
            ),
            ElevatedButton(
              onPressed: () {
                _saveImageToGallery();
                const snackbar = SnackBar(
                  content: Text('Picture saved!'),
                );
                ScaffoldMessenger.of(context).showSnackBar(snackbar);
              },
              child: const Text('Save to Gallery'),
            ),

          ],
        ),
      )
    );
  }
}
